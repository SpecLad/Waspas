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
class Program {
    std::unordered_map<std::string, const char *> parameters;

    friend class ProgramBuilder;
};

}

export
sem::Program
analyze(const nodes::Program &program_node, Reporter &reporter);
