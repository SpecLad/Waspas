module;

#include <memory>
#include <string>
#include <variant>
#include <vector>

export module semantics:expressions;

export import :core;
export import :types;

namespace sem {

class Expression {
public:
    virtual
    ~Expression() = default;

    virtual const Type &
    type(const Scope &scope) const = 0;
};

export // export to work around VC++ ICE
template <typename Base = Expression>
class ExpressionId : public Base {
public:
    ExpressionId(const std::string &id, std::size_t scope_index)
        : id_(id), scope_index_(scope_index) {}

    const std::string &
    id() const { return id_; }

    std::size_t
    scopeIndex() const { return scope_index_; }

private:
    std::string id_;
    std::size_t scope_index_;
};

class ExpressionBound final : public ExpressionId<> {
public:
    using ExpressionId::ExpressionId;

    const Type &
    type(const Scope &scope) const override;
};

class ExpressionConstant final : public Expression {
public:
    explicit
    ExpressionConstant(Constant::ptr_t constant)
        : constant_(constant) {}

    const Type &
    type(const Scope &) const override { return *constant_->type(); }

private:
    Constant::ptr_t constant_;
};

class ExpressionNil final : public Expression {
public:
    ExpressionNil() = default;

    const Type &
    type(const Scope &) const override;
};

export
class ExpressionOperatorBinary : public Expression {
public:
    ExpressionOperatorBinary(
        std::unique_ptr<sem::Expression> &&left,
        std::unique_ptr<sem::Expression> &&right
    ) : left_(std::move(left)), right_(std::move(right)) {}

private:
    std::unique_ptr<sem::Expression> left_;
    std::unique_ptr<sem::Expression> right_;
};

export
class ExpressionOperatorRelational : public ExpressionOperatorBinary {
    using ExpressionOperatorBinary::ExpressionOperatorBinary;

    const Type &
    type(const Scope &scope) const override { return TypeBoolean::instance(); }
};

export
class ExpressionOperatorEqual : public ExpressionOperatorRelational {
    using ExpressionOperatorRelational::ExpressionOperatorRelational;
};

export
class ExpressionOperatorGreater : public ExpressionOperatorRelational {
    using ExpressionOperatorRelational::ExpressionOperatorRelational;
};

export
class ExpressionOperatorGreaterOrEqual : public ExpressionOperatorRelational {
    using ExpressionOperatorRelational::ExpressionOperatorRelational;
};

export
class ExpressionOperatorIn : public ExpressionOperatorRelational {
    using ExpressionOperatorRelational::ExpressionOperatorRelational;
};

export
class ExpressionOperatorLess : public ExpressionOperatorRelational {
    using ExpressionOperatorRelational::ExpressionOperatorRelational;
};

export
class ExpressionOperatorLessOrEqual : public ExpressionOperatorRelational {
    using ExpressionOperatorRelational::ExpressionOperatorRelational;
};

export
class ExpressionOperatorNotEqual : public ExpressionOperatorRelational {
    using ExpressionOperatorRelational::ExpressionOperatorRelational;
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
    type(const Scope &scope) const override;

private:
    std::vector<member_designator_t> members_;
    std::shared_ptr<const TypeSetIncomplete> type_;
};

class VariableAccess : public Expression {
};

class VariableAccessActivationResult final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const Type &
    type(const Scope &scope) const override;
};

class VariableAccessFieldDesignatorId final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const Type &
    type(const Scope &scope) const override;
};

class VariableAccessParameterId final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const Type &
    type(const Scope &scope) const override;
};

class VariableAccessVariableId final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const Type &
    type(const Scope &scope) const override;
};

class VariableAccessBuffer final : public VariableAccess {
public:
    explicit
    VariableAccessBuffer(
        std::unique_ptr<VariableAccess> &&file
    ) : file_(std::move(file)) {}

    const Type &
    type(const Scope &scope) const override;

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
    type(const Scope &scope) const override;

private:
    std::unique_ptr<VariableAccess> pointer_;
};

class VariableAccessField final : public VariableAccess {
public:
    VariableAccessField(
        std::unique_ptr<VariableAccess> &&record,
        const std::string &field_name
    ) : record_(std::move(record)), field_name_(field_name) {}

    const Type &
    type(const Scope &scope) const override;

    const VariableAccess &
    record() const { return *record_; }

    const std::string &
    fieldName() const { return field_name_; }

private:
    std::unique_ptr<VariableAccess> record_;
    std::string field_name_;
};

class VariableAccessIndexed final : public VariableAccess {
public:
    VariableAccessIndexed(
        std::unique_ptr<VariableAccess> &&array,
        std::unique_ptr<Expression> &&index
    ) : array_(std::move(array)), index_(std::move(index)) {}

    const Type &
    type(const Scope &scope) const override;

    const VariableAccess &
    array() const { return *array_; }

private:
    std::unique_ptr<VariableAccess> array_;
    std::unique_ptr<Expression> index_;
};

export
class SubroutineReference {
public:
    enum Kind { REGULAR, PARAMETER };

    SubroutineReference(const std::string &id, std::size_t scope_index, Kind kind)
        : id_(id), scope_index_(scope_index), kind_(kind) {}

    const std::string &
    id() const { return id_; }

    std::size_t
    scopeIndex() const { return scope_index_; }

private:
    std::string id_;
    std::size_t scope_index_;
    Kind kind_;
};

export
using actual_parameter_section_t = std::variant<
    std::vector<std::unique_ptr<Expression>>, // value parameters
    std::vector<std::unique_ptr<VariableAccess>>, // variable parameters
    SubroutineReference // procedure/function parameter
>;

}
