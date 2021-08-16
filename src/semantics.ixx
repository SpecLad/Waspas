export module semantics;

import parsing;
import reporting;

class
ProgramBuilder;

namespace sem {

export
class Program {
    friend class ProgramBuilder;
};

}

export
sem::Program
analyze(const nodes::Program &program_node, Reporter &reporter);
