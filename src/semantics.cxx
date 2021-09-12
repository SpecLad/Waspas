module;

#include <memory>
#include <string>
#include <unordered_map>

module semantics;

const char *
sem::Block::findDefiningPoint(std::string_view identifier) const {
    auto it = constants_.find(std::string(identifier));
    if (it != constants_.end()) return it->second.location();

    return nullptr;
}

class ProgramBuilder {
public:
    ProgramBuilder(Reporter &reporter) : reporter_(reporter)
    {}

    void
    buildBlock(const nodes::Block &block_node, sem::Block &block) {
        for (auto &label_node : block_node.label_declarations) {
            auto label_location = label_node.view.data();

            auto [it, success] = block.labels_.try_emplace(
                label_node.value, label_location);

            if (!success) {
                reporter_.err(label_location, "duplicate-label",
                    "label \"{}\" already defined", label_node.value);
                reporter_.note(it->second,
                    "defining point of \"{}\"", label_node.value);
            }

            // TODO: verify that each label is used exactly once
            // in the block where it's defined
        }

        for (auto &constant_def_node : block_node.constant_definitions) {
            auto constant_location = constant_def_node.view.data();

            std::string_view constant_name = constant_def_node.name.spelling;

            if (const char *previous_defining_point
                = block.findDefiningPoint(constant_name)
            ) {
                reporter_.err(constant_location, "duplicate-identifier",
                    "identifier \"{}\" already defined", constant_name);
                reporter_.note(previous_defining_point,
                    "defining point of \"{}\"", constant_name);
                continue;
            }

            sem::Constant constant;
            constant.location_ = constant_location;

            auto &constant_value_node = *constant_def_node.value;
            auto constant_value_location = constant_value_node.view.data();

            if (auto *signed_const_node = dynamic_cast<nodes::SignedConstant*>(&constant_value_node)) {
                if (signed_const_node->sign != nodes::Sign::NONE)
                    reporter_.err(constant_value_location, "unsupported",
                        "signs are not supported yet");

                if (auto *unsigned_integer_node
                    = dynamic_cast<nodes::UnsignedIntegerConstant *>(
                        signed_const_node->unsigned_value.get())
                ) {
                    constant.value_ = std::make_unique<sem::ConstantValueInteger>(
                        unsigned_integer_node->value);
                }
            }

            if (!constant.value_) {
                reporter_.err(constant_value_location, "unsupported", "constant type not yet supported");
            }

            block.constants_.emplace(constant_name, std::move(constant));
        }
    }

    sem::Program
    build(const nodes::Program &program_node) {
        sem::Program program;

        for (auto &parameter_node : program_node.parameter_declarations) {
            auto parameter_location = parameter_node.view.data();

            auto [it, success] = program.parameters_.try_emplace(
                parameter_node.spelling, parameter_location);

            if (!success) {
                reporter_.err(parameter_location, "duplicate-program-parameter",
                    "program parameter \"{}\" already defined", parameter_node.spelling);
                reporter_.note(it->second,
                    "defining point of \"{}\"", parameter_node.spelling);
            }
        }

        // TODO: check that program parameters correspond to variables

        buildBlock(program_node.block, program.block_);

        return program;
    }

private:
    Reporter &reporter_;
};

sem::Program
analyze(const nodes::Program &program_node, Reporter &reporter) {
    return ProgramBuilder(reporter).build(program_node);
}
