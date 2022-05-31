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
    virtual
    ~Type() = default;

    virtual std::string
    str() const = 0;
};

template <typename T>
class TypeBuiltin : public Type {
public:
    TypeBuiltin(const TypeBuiltin &) = delete;
    TypeBuiltin &operator =(const TypeBuiltin &) = delete;

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

class Constant;

export
class TypeSubrange : public Type {
public:
    TypeSubrange(
        std::shared_ptr<const Constant> smallest_value,
        std::shared_ptr<const Constant> largest_value
    ) : smallest_value_(smallest_value), largest_value_(largest_value) {}

    std::string
    str() const override { return "<subrange>"s; } // TODO: stringify the constants

private:
    std::shared_ptr<const Constant> smallest_value_;
    std::shared_ptr<const Constant> largest_value_;
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
template <typename T, typename Value>
class ConstantImpl : public Constant {
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
    : public ConstantImpl<ConstantInteger, pascal_integer_t>
{
    using ConstantImpl::ConstantImpl;

    std::string
    typeStr() const override { return "integer"s; }
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
    : public ConstantImpl<ConstantChar, char>
{
    using ConstantImpl::ConstantImpl;

    std::string
    typeStr() const override { return "char"s; }
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
