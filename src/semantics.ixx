module;

#include <cassert>
#include <format>
#include <memory>
#include <string>
#include <unordered_map>

export module semantics;

import parsing;
import reporting;

using namespace std::literals;

class ProgramBuilder;
struct BuiltinBlockInitializer;

namespace sem {

export
class Type {
public:
    Type() = default;

    virtual
    ~Type() = default;

    Type(const Type &) = delete;
    Type &operator =(const Type &) = delete;

    virtual std::string
    str() const = 0;

    virtual bool
    isOrdinal() const { return false; }

    virtual bool
    canBeFileComponent() const { return true; }
};

template <typename T, typename Base = Type>
class TypeBuiltin : public Base {
public:
    using Base::Base;

    static const T &
    instance() {
        static const T t;
        return t;
    }

    std::string
    str() const override { return std::string(T::NAME); }
};

export
class TypeBoolean final : public TypeBuiltin<TypeBoolean> {
public:
    static inline constexpr std::string_view NAME = "boolean"sv;

    virtual bool
    isOrdinal() const { return true; }

private:
    TypeBoolean() = default;
    friend class TypeBuiltin;
};

export
class TypeChar final : public TypeBuiltin<TypeChar> {
public:
    static inline constexpr std::string_view NAME = "char"sv;

    virtual bool
    isOrdinal() const { return true; }

private:
    TypeChar() = default;
    friend class TypeBuiltin;
};

export
class TypeInteger final : public TypeBuiltin<TypeInteger> {
public:
    static inline constexpr std::string_view NAME = "integer"sv;

    virtual bool
    isOrdinal() const { return true; }

private:
    TypeInteger() = default;
    friend class TypeBuiltin;
};

export
class TypeReal final : public TypeBuiltin<TypeReal> {
public:
    static inline constexpr std::string_view NAME = "real"sv;

private:
    TypeReal() = default;
    friend class TypeBuiltin;
};

class ConstantOrdinal;

export
class TypeSubrange final : public Type {
public:
    TypeSubrange(
        std::shared_ptr<const ConstantOrdinal> smallest_value,
        std::shared_ptr<const ConstantOrdinal> largest_value
    ) : smallest_value_(smallest_value), largest_value_(largest_value) {}

    std::string
    str() const override;

    virtual bool
    isOrdinal() const { return true; }

private:
    std::shared_ptr<const ConstantOrdinal> smallest_value_;
    std::shared_ptr<const ConstantOrdinal> largest_value_;
};

export
class TypeArray final : public Type {
public:
    TypeArray(
        std::shared_ptr<const Type> index_type,
        std::shared_ptr<const Type> component_type,
        bool is_packed
    ) : index_type_(index_type), component_type_(component_type), is_packed_(is_packed) {}

    std::string
    str() const override {
        return (is_packed_ ? "packed "s : ""s)
            + "array ["s + index_type_->str() + "] of " + component_type_->str();
    }

    bool
    canBeFileComponent() const override {
        return component_type_->canBeFileComponent();
    }

private:
    std::shared_ptr<const Type> index_type_;
    std::shared_ptr<const Type> component_type_;
    bool is_packed_;
};

export
class TypeFile : public Type {
public:
    TypeFile(
        std::shared_ptr<const Type> component_type,
        bool is_packed
    ) : component_type_(component_type), is_packed_(is_packed) {
        assert(component_type_->canBeFileComponent());
    }

    std::string
    str() const override {
        return (is_packed_ ? "packed "s : ""s)
            + "file of "s + component_type_->str();
    }

    bool
    canBeFileComponent() const override {
        return false;
    }

private:
    std::shared_ptr<const Type> component_type_;
    bool is_packed_;
};

export
class TypeSet final : public Type {
public:
    TypeSet(
        std::shared_ptr<const Type> base_type,
        bool is_packed
    ) : base_type_(base_type), is_packed_(is_packed) {
        assert(base_type->isOrdinal());
    }

    std::string
    str() const override {
        return (is_packed_ ? "packed "s : ""s)
            + "set of "s + base_type_->str();
    }

private:
    std::shared_ptr<const Type> base_type_;
    bool is_packed_;
};

export
class TypeText final : public TypeBuiltin<TypeText, TypeFile> {
public:
    static inline constexpr std::string_view NAME = "text"sv;

private:
    TypeText()
        : TypeBuiltin(
            std::shared_ptr<const Type>(std::shared_ptr<void>(), &TypeChar::instance()),
            true
        )
    {}

    friend class TypeBuiltin;
};

export
class Constant {
public:
    virtual
    ~Constant() = default;

    virtual const Type &
    type() const = 0;
};

export
class ConstantOrdinal : public Constant {
public:
    virtual std::string
    str() const = 0;

    virtual pascal_integer_t
    ordinalNumber() const = 0;
};

export
template <typename T, typename Value, typename Base = Constant>
class ConstantImpl : public Base {
public:
    explicit
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
public:
    const TypeBoolean &
    type() const override { return TypeBoolean::instance(); }

    std::string
    str() const override { return value_ ? "true"s : "false"s; }

    pascal_integer_t
    ordinalNumber() const override { return value_ ? 1 : 0; }

    static const ConstantBoolean &
    instanceFalse() {
        static const ConstantBoolean c(false);
        return c;
    }

    static const ConstantBoolean &
    instanceTrue() {
        static const ConstantBoolean c(true);
        return c;
    }

private:
    explicit ConstantBoolean(bool value) : ConstantImpl(value) {}
};

export
class ConstantInteger final
    : public ConstantImpl<ConstantInteger, pascal_integer_t, ConstantOrdinal>
{
    using ConstantImpl::ConstantImpl;

    const TypeInteger &
    type() const override { return TypeInteger::instance(); }

    std::string
    str() const override { return std::to_string(value_); }

    pascal_integer_t
    ordinalNumber() const override { return value_; }
};

export
class ConstantReal final
    : public ConstantImpl<ConstantReal, pascal_real_t>
{
    using ConstantImpl::ConstantImpl;

    const TypeReal &
    type() const override { return TypeReal::instance(); }
};

export
class ConstantChar final
    : public ConstantImpl<ConstantChar, char, ConstantOrdinal>
{
    using ConstantImpl::ConstantImpl;

    const TypeChar &
    type() const override { return TypeChar::instance(); }

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
        , type_(
            std::make_shared<TypeSubrange>(
                std::make_shared<ConstantInteger>(1),
                std::make_shared<ConstantInteger>(pascal_integer_t(value.size()))
            ),
            std::shared_ptr<const Type>(std::shared_ptr<void>(), &TypeChar::instance()),
            true
        )
    {
        assert(value.size() <= std::size_t(PASCAL_INTEGER_MAX));
    }

    const TypeArray &
    type() const override {
        return type_;
    }

private:
    TypeArray type_;
};

struct DefiningOccurrence {
    const char *location;
    enum Kind { NOT_TYPE, TYPE } kind;
};

using defining_occurrences_t = std::unordered_map<std::string, DefiningOccurrence>;

export
class Block {
    Block *parent_{};

    defining_occurrences_t defining_occurrences_;

    std::unordered_map<pascal_integer_t, const char *> labels_;
    std::unordered_map<std::string, std::shared_ptr<const Constant>> constants_;
    std::unordered_map<std::string, std::shared_ptr<const Type>> types_;

    friend class ProgramBuilder;
    friend struct BuiltinBlockInitializer;
};

export
class Program {
    std::unordered_map<std::string, const char *> parameters_;

    Block block_;

    friend class ProgramBuilder;
};

}

export
sem::Program
analyze(const nodes::Program &program_node, Reporter &reporter);
