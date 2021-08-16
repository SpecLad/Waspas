module;

#include <string>
#include <unordered_set>

module semantics;

class ProgramBuilder {
public:
    ProgramBuilder(Reporter &reporter) : reporter_(reporter)
    {}

    sem::Program
    build(const nodes::Program &program_node) {
        sem::Program program;

        for (auto &parameter_node : program_node.parameter_declarations) {
            auto [it, success] = program.parameters.insert(parameter_node.spelling);
            if (!success)
                reporter_.err(parameter_node.view.data(), "duplicate-program-parameter",
                    "duplicate program parameter \"{}\"", parameter_node.spelling);
        }

        // TODO: check that program parameters correspond to variables

        return program;
    }

private:
    Reporter &reporter_;
};

sem::Program
analyze(const nodes::Program &program_node, Reporter &reporter) {
    return ProgramBuilder(reporter).build(program_node);
}
