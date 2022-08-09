module;

#include <memory>
#include <string>

export module semantics:expressions;

export import :core;

namespace sem {

class Expression {
public:
    virtual
    ~Expression() = default;

    virtual const DynamicType &
    type(const Scope &scope) const = 0;
};

export
class ActualParameterSection {}; // TODO

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

    const DynamicType &
    type(const Scope &scope) const override;
};

class ExpressionConstant final : public Expression {
public:
    explicit
    ExpressionConstant(Constant::ptr_t constant)
        : constant_(constant) {}

    const DynamicType &
    type(const Scope &) const override { return *constant_->type(); }

private:
    Constant::ptr_t constant_;
};

class ExpressionNil final : public Expression {
public:
    ExpressionNil() = default;

    const DynamicType &
    type(const Scope &) const override;
};

class VariableAccess : public Expression {
};

class VariableAccessActivationResult final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const DynamicType &
    type(const Scope &scope) const override;
};

class VariableAccessFieldDesignatorId final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const DynamicType &
    type(const Scope &scope) const override;
};

class VariableAccessParameterId final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const DynamicType &
    type(const Scope &scope) const override;
};

class VariableAccessVariableId final : public ExpressionId<VariableAccess> {
public:
    using ExpressionId::ExpressionId;

    const DynamicType &
    type(const Scope &scope) const override;
};

class VariableAccessBuffer final : public VariableAccess {
public:
    explicit
    VariableAccessBuffer(
        std::unique_ptr<VariableAccess> &&file
    ) : file_(std::move(file)) {}

    const DynamicType &
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

    const DynamicType &
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

    const DynamicType &
    type(const Scope &scope) const override;

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

    const DynamicType &
    type(const Scope &scope) const override;

private:
    std::unique_ptr<VariableAccess> array_;
    std::unique_ptr<Expression> index_;
};

class VariableAccessIndexedDynamic final : public VariableAccess {
public:
    VariableAccessIndexedDynamic(
        std::unique_ptr<VariableAccess> &&dynamic_array,
        std::unique_ptr<Expression> &&index
    ) : dynamic_array_(std::move(dynamic_array)), index_(std::move(index)) {}

    const DynamicType &
    type(const Scope &scope) const override;

private:
    std::unique_ptr<VariableAccess> dynamic_array_;
    std::unique_ptr<Expression> index_;
};

}
