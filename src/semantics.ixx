module;

#include <unordered_set>
#include <string>

export module semantics;

import parsing;
import reporting;

class
ProgramBuilder;

namespace sem {

export
class Program {
    std::unordered_set<std::string> parameters;

    friend class ProgramBuilder;
};

}

export
sem::Program
analyze(const nodes::Program &program_node, Reporter &reporter);
