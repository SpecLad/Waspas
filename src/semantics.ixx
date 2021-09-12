module;

#include <unordered_map>
#include <string>

export module semantics;

import parsing;
import reporting;

class
ProgramBuilder;

namespace sem {

export
class Constant {
public:
    const char *
    location() const { return location_; }

private:
    const char *location_;

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
