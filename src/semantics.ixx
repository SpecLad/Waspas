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

export
class TypeInteger : public Type {
public:
    std::string
    str() const { return "integer"s; }

    static const TypeInteger &
    instance() {
        static const TypeInteger t;
        return t;
    }

private:
    TypeInteger() = default;
};

export
class ConstantValue {
public:
    virtual
    ~ConstantValue() = default;

    virtual std::string
    typeStr() const = 0;
};

export
template <typename T, typename Value>
class ConstantValueImpl : public ConstantValue {
public:
    explicit
    ConstantValueImpl(const Value &value) : value_(value)
    {}

    Value
    value() const { return value_; }

protected:
    Value value_;
};

export
class ConstantValueInteger final
    : public ConstantValueImpl<ConstantValueInteger, pascal_integer_t>
{
    using ConstantValueImpl::ConstantValueImpl;

    std::string
    typeStr() const override { return "integer"s; }
};

export
class ConstantValueReal final
    : public ConstantValueImpl<ConstantValueReal, pascal_real_t>
{
    using ConstantValueImpl::ConstantValueImpl;

    std::string
    typeStr() const override { return "real"s; }
};

export
class ConstantValueChar final
    : public ConstantValueImpl<ConstantValueChar, char>
{
    using ConstantValueImpl::ConstantValueImpl;

    std::string
    typeStr() const override { return "char"s; }
};

export
class ConstantValueString final
    : public ConstantValueImpl<ConstantValueString, std::string>
{
    using ConstantValueImpl::ConstantValueImpl;

    std::string
    typeStr() const override {
        return std::format("packed array(1..{}) of char", value_.size());
    }
};

export
class Constant {
    const char *location_{};
    std::shared_ptr<const ConstantValue> value_;

    friend class ProgramBuilder;
    friend struct BuiltinBlockInitializer;
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
    std::unordered_map<std::string, Constant> constants_;
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
