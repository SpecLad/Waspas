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
