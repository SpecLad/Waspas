module;

#include <memory>
#include <string>
#include <unordered_map>

export module semantics;

import parsing;
import reporting;

class
ProgramBuilder;

namespace sem {

export
class ConstantValue {
public:
    virtual
    ~ConstantValue() = default;
};

export
class ConstantValueInteger : public ConstantValue {
public:
    explicit
    ConstantValueInteger(pascal_integer_t value) : value_(value)
    {}

    pascal_integer_t
    value() const { return value_; }

private:
    pascal_integer_t value_;
};

export
class Constant {
public:
    const char *
    location() const { return location_; }

private:
    const char *location_;
    std::unique_ptr<ConstantValue> value_;

    friend class ProgramBuilder;
};

export
class Block {
    std::unordered_map<pascal_integer_t, const char *> labels_;
    std::unordered_map<std::string, Constant> constants_;

    const char *
    findDefiningPoint(std::string_view identifier) const;

    friend class ProgramBuilder;
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
