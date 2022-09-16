module;

#include <cassert>
#include <memory>
#include <string>

export module semantics:constants;

export import :core;
export import :types;

import parsing;

using namespace std::literals;

namespace sem {

export
template <typename T, typename Value, typename Base = Constant>
class ConstantImpl : public Base {
public:
    explicit constexpr
    ConstantImpl(const Value &value) : value_(value)
    {}

    Value
    value() const { return value_; }

protected:
    Value value_;
};

export
class ConstantBoolean final
    : public ConstantImpl<ConstantBoolean, bool, ConstantOrdinal>
{
private:
    explicit constexpr
    ConstantBoolean(bool value) : ConstantImpl(value) {}

public:
    TypeOrdinal::ptr_t
    typeOrdinal() const override { return staticPtr(TypeBoolean::instance()); }

    std::string
    str() const override { return value_ ? "true"s : "false"s; }

    pascal_integer_t
    ordinalNumber() const override { return value_ ? 1 : 0; }

    static const ConstantBoolean &
    instanceFalse() {
        static constexpr ConstantBoolean c(false);
        return c;
    }

    static const ConstantBoolean &
    instanceTrue() {
        static constexpr ConstantBoolean c(true);
        return c;
    }
};

export
class ConstantInteger final
    : public ConstantImpl<ConstantInteger, pascal_integer_t, ConstantOrdinal>
{
public:
    explicit constexpr
    ConstantInteger(pascal_integer_t value) : ConstantImpl(value) {
        // Standard Pascal does not support integers lower than -maxint.
        assert(value >= -PASCAL_INTEGER_MAX);
    }

    TypeOrdinal::ptr_t
    typeOrdinal() const override { return staticPtr(TypeInteger::instance()); }

    std::string
    str() const override { return std::to_string(value_); }

    pascal_integer_t
    ordinalNumber() const override { return value_; }

    static const ConstantInteger &
    instanceMax() {
        static constexpr ConstantInteger c(PASCAL_INTEGER_MAX);
        return c;
    }
};

export
class ConstantReal final
    : public ConstantImpl<ConstantReal, pascal_real_t>
{
    using ConstantImpl::ConstantImpl;

    Type::ptr_t
    type() const override { return staticPtr(TypeReal::instance()); }
};

export
class ConstantChar final
    : public ConstantImpl<ConstantChar, char, ConstantOrdinal>
{
    using ConstantImpl::ConstantImpl;

    TypeOrdinal::ptr_t
    typeOrdinal() const override { return staticPtr(TypeChar::instance()); }

    std::string
    str() const override {
        if (value_ == '\'')
            return "''''"s;
        else
            return "'"s + value_ + "'";
    }

    pascal_integer_t
    ordinalNumber() const override { return pascal_integer_t(value_); }
};

export
class ConstantString final
    : public ConstantImpl<ConstantString, std::string>
{
public:
    explicit
    ConstantString(const std::string &value)
        : ConstantImpl(value)
        , type_(std::make_shared<TypeArray>(
            std::make_shared<TypeSubrange>(
                std::make_shared<ConstantInteger>(1),
                std::make_shared<ConstantInteger>(pascal_integer_t(value.size()))
            ),
            staticPtr(TypeChar::instance()),
            true
        ))
    {
        assert(value.size() <= std::size_t(PASCAL_INTEGER_MAX));
    }

    Type::ptr_t
    type() const override {
        return type_;
    }

private:
    std::shared_ptr<const TypeArray> type_;
};

export
class ConstantEnumerated final : public ConstantOrdinal
{
public:
    TypeOrdinal::ptr_t
    typeOrdinal() const override { return type_.shared_from_this(); }

    std::string
    str() const override { return name_; }

    pascal_integer_t
    ordinalNumber() const override { return ordinal_number_; }

private:
    explicit
    ConstantEnumerated(
        const TypeEnumerated &type,
        pascal_integer_t ordinal_number,
        const std::string &name
    )
        : type_(type), ordinal_number_(ordinal_number), name_(name)
    {}

    const TypeEnumerated &type_;
    pascal_integer_t ordinal_number_;
    std::string name_;

    friend class TypeEnumerated;
};

}
