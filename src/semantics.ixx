module;

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
};

template <typename T>
class TypeBuiltin : public Type {
public:
    static const T &
        instance() {
        static const T t;
        return t;
    }

    std::string
    str() const override { return std::string(T::NAME); }

protected:
    TypeBuiltin() = default;
};

export
class TypeBoolean : public TypeBuiltin<TypeBoolean> {
public:
    static inline constexpr std::string_view NAME = "boolean"sv;

private:
    TypeBoolean() = default;
    friend class TypeBuiltin<TypeBoolean>;
};

export
class TypeChar : public TypeBuiltin<TypeChar> {
public:
    static inline constexpr std::string_view NAME = "char"sv;

private:
    TypeChar() = default;
    friend class TypeBuiltin<TypeChar>;
};

export
class TypeInteger : public TypeBuiltin<TypeInteger> {
public:
    static inline constexpr std::string_view NAME = "integer"sv;

private:
    TypeInteger() = default;
    friend class TypeBuiltin<TypeInteger>;
};

export
class TypeReal : public TypeBuiltin<TypeReal> {
public:
    static inline constexpr std::string_view NAME = "real"sv;

private:
    TypeReal() = default;
    friend class TypeBuiltin<TypeReal>;
};

export
class TypeText : public TypeBuiltin<TypeText> {
public:
    static inline constexpr std::string_view NAME = "text"sv;

private:
    TypeText() = default;
    friend class TypeBuiltin<TypeText>;
};

class ConstantOrdinal;

export
class TypeSubrange : public Type {
public:
    TypeSubrange(
        std::shared_ptr<const ConstantOrdinal> smallest_value,
        std::shared_ptr<const ConstantOrdinal> largest_value
    ) : smallest_value_(smallest_value), largest_value_(largest_value) {}

    std::string
    str() const override;

private:
    std::shared_ptr<const ConstantOrdinal> smallest_value_;
    std::shared_ptr<const ConstantOrdinal> largest_value_;
};

export
class TypeArray : public Type {
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

private:
    std::shared_ptr<const Type> index_type_;
    std::shared_ptr<const Type> component_type_;
    bool is_packed_;
};

export
class Constant {
public:
    virtual
    ~Constant() = default;

    virtual std::string
    typeStr() const = 0;
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
class ConstantInteger final
    : public ConstantImpl<ConstantInteger, pascal_integer_t, ConstantOrdinal>
{
    using ConstantImpl::ConstantImpl;

    std::string
    typeStr() const override { return "integer"s; }

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

    std::string
    typeStr() const override { return "real"s; }
};

export
class ConstantChar final
    : public ConstantImpl<ConstantChar, char, ConstantOrdinal>
{
    using ConstantImpl::ConstantImpl;

    std::string
    typeStr() const override { return "char"s; }

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
    using ConstantImpl::ConstantImpl;

    std::string
    typeStr() const override {
        return std::format("packed array(1..{}) of char", value_.size());
    }
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
