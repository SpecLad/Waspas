// SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
//
// SPDX-License-Identifier: MPL-2.0

module;

#include <memory>
#include <string>
#include <variant>
#include <vector>

export module semantics:expressions;

export import :core;
export import :types;

namespace sem {

export
class SubroutineReference {
public:
    enum Kind { REGULAR, PARAMETER };

    SubroutineReference(const Cisref &id, std::size_t scope_index, Kind kind)
        : id_(id), scope_index_(scope_index), kind_(kind) {}

    Cisref
    id() const { return id_; }

    std::size_t
    scopeIndex() const { return scope_index_; }

    Kind
    kind() const { return kind_; }

private:
    Cisref id_;
    std::size_t scope_index_;
    Kind kind_;
};

class Expression;
class VariableAccess;

export
using actual_parameter_section_t = std::variant<
    std::vector<std::unique_ptr<Expression>>, // value parameters
    std::vector<std::unique_ptr<VariableAccess>>, // variable parameters
    SubroutineReference // procedure/function parameter
>;

class Expression {
public:
    virtual
    ~Expression() = default;

    virtual const Type &
    valueType(const Scope &scope) const = 0;
};

template <typename Base = Expression>
class ExpressionId : public Base {
public:
    ExpressionId(const Cisref &id, std::size_t scope_index)
        : id_(id), scope_index_(scope_index) {}

    Cisref
    id() const { return id_; }

    std::size_t
    scopeIndex() const { return scope_index_; }

private:
    Cisref id_;
    std::size_t scope_index_;
};

class ExpressionBound final : public ExpressionId<> {
public:
    using ExpressionId::ExpressionId;

    const Type &
    valueType(const Scope &scope) const override;
};

class ExpressionConstant final : public Expression {
public:
    explicit
    ExpressionConstant(Constant::ptr_t constant)
        : constant_(constant) {}

    const Type &
    valueType(const Scope &) const override { return *constant_->type(); }

private:
    Constant::ptr_t constant_;
};

export
class ExpressionFunctionDesignator : public Expression {
public:
    explicit
    ExpressionFunctionDesignator(
        const SubroutineReference &reference,
        std::vector<sem::actual_parameter_section_t> &&actual_parameters
    );

    ~ExpressionFunctionDesignator();

    const Type &
    valueType(const Scope &scope) const override;

private:
    SubroutineReference reference_;
    std::vector<sem::actual_parameter_section_t> actual_parameters_;
};

class ExpressionNil final : public Expression {
public:
    ExpressionNil() = default;

    const Type &
    valueType(const Scope &) const override {
        return TypePointerAny::instance();
    }
};

class ExpressionOperatorBinary : public Expression {
public:
    ExpressionOperatorBinary(
        std::unique_ptr<sem::Expression> &&left,
        std::unique_ptr<sem::Expression> &&right
    ) : left_(std::move(left)), right_(std::move(right)) {}

    const sem::Expression &
    left() const { return *left_; }

    const sem::Expression &
    right() const { return *right_; }

private:
    std::unique_ptr<sem::Expression> left_;
    std::unique_ptr<sem::Expression> right_;
};

template <typename T>
class ExpressionOperatorSingleType : public ExpressionOperatorBinary {
    using ExpressionOperatorBinary::ExpressionOperatorBinary;

    const Type &
    valueType(const Scope &) const override { return T::instance(); }
};

using ExpressionOperatorBoolean = ExpressionOperatorSingleType<sem::TypeBoolean>;

export
class ExpressionOperatorAnd : public ExpressionOperatorBoolean {
    using ExpressionOperatorBoolean::ExpressionOperatorBoolean;
};

export
class ExpressionOperatorEqual : public ExpressionOperatorBoolean {
    using ExpressionOperatorBoolean::ExpressionOperatorBoolean;
};

export
class ExpressionOperatorGreater : public ExpressionOperatorBoolean {
    using ExpressionOperatorBoolean::ExpressionOperatorBoolean;
};

export
class ExpressionOperatorGreaterOrEqual : public ExpressionOperatorBoolean {
    using ExpressionOperatorBoolean::ExpressionOperatorBoolean;
};

export
class ExpressionOperatorIn : public ExpressionOperatorBoolean {
    using ExpressionOperatorBoolean::ExpressionOperatorBoolean;
};

export
class ExpressionOperatorLess : public ExpressionOperatorBoolean {
    using ExpressionOperatorBoolean::ExpressionOperatorBoolean;
};

export
class ExpressionOperatorLessOrEqual : public ExpressionOperatorBoolean {
    using ExpressionOperatorBoolean::ExpressionOperatorBoolean;
};

export
class ExpressionOperatorNotEqual : public ExpressionOperatorBoolean {
    using ExpressionOperatorBoolean::ExpressionOperatorBoolean;
};

export
class ExpressionOperatorOr : public ExpressionOperatorBoolean {
    using ExpressionOperatorBoolean::ExpressionOperatorBoolean;
};

class ExpressionOperatorCommonType : public ExpressionOperatorBinary {
    using ExpressionOperatorBinary::ExpressionOperatorBinary;

    const Type &
    valueType(const Scope &scope) const override;
};

export
class ExpressionOperatorAdd : public ExpressionOperatorCommonType {
    using ExpressionOperatorCommonType::ExpressionOperatorCommonType;
};

export
class ExpressionOperatorMultiply : public ExpressionOperatorCommonType {
    using ExpressionOperatorCommonType::ExpressionOperatorCommonType;
};

export
class ExpressionOperatorSubtract : public ExpressionOperatorCommonType {
    using ExpressionOperatorCommonType::ExpressionOperatorCommonType;
};

export
class ExpressionOperatorDivideReal : public ExpressionOperatorSingleType<TypeReal> {
    using ExpressionOperatorSingleType::ExpressionOperatorSingleType;
};

export
class ExpressionOperatorDivideInteger : public ExpressionOperatorSingleType<TypeInteger> {
    using ExpressionOperatorSingleType::ExpressionOperatorSingleType;
};

export
class ExpressionOperatorModulo : public ExpressionOperatorSingleType<TypeInteger> {
    using ExpressionOperatorSingleType::ExpressionOperatorSingleType;
};

class ExpressionOperatorUnary : public Expression {
public:
    explicit
    ExpressionOperatorUnary(
        std::unique_ptr<sem::Expression> &&operand
    ) : operand_(std::move(operand)) {}

    const sem::Expression &
    operand() const { return *operand_; }

private:
    std::unique_ptr<sem::Expression> operand_;
};

export
class ExpressionOperatorNegate : public ExpressionOperatorUnary {
public:
    using ExpressionOperatorUnary::ExpressionOperatorUnary;

    const Type &
    valueType(const Scope &scope) const override { return operand().valueType(scope); }
};

export
class ExpressionOperatorNot : public ExpressionOperatorUnary {
public:
    using ExpressionOperatorUnary::ExpressionOperatorUnary;

    const Type &
    valueType(const Scope &) const override { return sem::TypeBoolean::instance(); }
};

using member_designator_t = std::variant<
    std::unique_ptr<Expression>,
    std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>
>;

export
class ExpressionSetConstructor : public Expression {
public:
    ExpressionSetConstructor() {}

    explicit
    ExpressionSetConstructor(
        std::vector<member_designator_t> &&members,
        const TypeOrdinal &member_type
    )
        : members_(std::move(members))
        , type_(std::make_shared<TypeSetIncomplete>(member_type))
    {}

    const Type &
    valueType(const Scope &) const override {
        if (type_) return *type_;
        return TypeSetAny::instance();
    }

private:
    std::vector<member_designator_t> members_;
    std::shared_ptr<const TypeSetIncomplete> type_;
};

class VariableAccess : public Expression {
public:
    virtual const Type &
    variableType(const Scope &scope) const = 0;

    const Type &
    valueType(const Scope &scope) const override { return variableType(scope).promoted(); }
};

class VariableAccessActivationResult final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const Type &
    variableType(const Scope &scope) const override;
};

class VariableAccessFieldDesignatorId final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const Type &
    variableType(const Scope &scope) const override;
};

class VariableAccessParameterId final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const Type &
    variableType(const Scope &scope) const override;
};

class VariableAccessVariableId final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const Type &
    variableType(const Scope &scope) const override;
};

class VariableAccessBuffer final : public VariableAccess {
public:
    explicit
    VariableAccessBuffer(
        std::unique_ptr<VariableAccess> &&file
    ) : file_(std::move(file)) {}

    const Type &
    variableType(const Scope &scope) const override;

private:
    std::unique_ptr<VariableAccess> file_;
};

class VariableAccessDereference final : public VariableAccess {
public:
    explicit
    VariableAccessDereference(
        std::unique_ptr<VariableAccess> &&pointer
    ) : pointer_(std::move(pointer)) {}

    const Type &
    variableType(const Scope &scope) const override;

private:
    std::unique_ptr<VariableAccess> pointer_;
};

class VariableAccessField final : public VariableAccess {
public:
    VariableAccessField(
        std::unique_ptr<VariableAccess> &&record,
        const Cisref &field_name
    ) : record_(std::move(record)), field_name_(field_name) {}

    const Type &
    variableType(const Scope &scope) const override;

    const VariableAccess &
    record() const { return *record_; }

    Cisref
    fieldName() const { return field_name_; }

private:
    std::unique_ptr<VariableAccess> record_;
    Cisref field_name_;
};

class VariableAccessIndexed final : public VariableAccess {
public:
    VariableAccessIndexed(
        std::unique_ptr<VariableAccess> &&array,
        std::unique_ptr<Expression> &&index
    ) : array_(std::move(array)), index_(std::move(index)) {}

    const Type &
    variableType(const Scope &scope) const override;

    const VariableAccess &
    array() const { return *array_; }

private:
    std::unique_ptr<VariableAccess> array_;
    std::unique_ptr<Expression> index_;
};


}
