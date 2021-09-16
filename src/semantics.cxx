module;

#include <memory>
#include <string>
#include <unordered_map>

module semantics;

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

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
    applySignToConstantValue(std::unique_ptr<sem::ConstantValue> &v, nodes::Sign sign, const char *location) {
        if (sign == nodes::Sign::NONE) return;

        if (auto *p_integer_value = dynamic_cast<sem::ConstantValueInteger *>(v.get())) {
            if (sign == nodes::Sign::MINUS) {
                if (p_integer_value->value() == std::numeric_limits<pascal_integer_t>::min()) {
                    // It should be impossible to reach this, since integer constants can only
                    // be defined with unsigned literals and negations, which can't produce
                    // the lowest integer. But just in case, we'll handle it anyway.
                    reporter_.err(location, "invalid-negation",
                        "can't negate the lowest possible integer");
                }

                v.reset(new sem::ConstantValueInteger(-p_integer_value->value()));
            }
        }
        else if (auto *p_real_value = dynamic_cast<sem::ConstantValueReal *>(v.get())) {
            if (sign == nodes::Sign::MINUS)
                v.reset(new sem::ConstantValueReal(-p_real_value->value()));
        }
        else {
            reporter_.err(location, "type-mismatch",
                "a sign cannot be applied to a constant of type \"{}\"", v->typeStr());
        }
    }

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

            visit(constant_value_node, overloaded{
                [&, this](nodes::SignedConstant &sc_node) {
                    visit(*sc_node.unsigned_value, overloaded{
                        [&](nodes::UnsignedIntegerConstant &uic_node) {
                            constant.value_.reset(new sem::ConstantValueInteger(
                                uic_node.value));
                        },
                        [&](nodes::UnsignedRealConstant &urc_node) {
                            constant.value_.reset(new sem::ConstantValueReal(
                                urc_node.value));
                        },
                        [&](nodes::Identifier &id_node) {
                            // TODO: add proper ID resolving: add parent scope searching;
                            // builtins; checks that we don't use an ID before it's defined
                            auto it = block.constants_.find(id_node.spelling);

                            if (it == block.constants_.end()) {
                                reporter_.err(id_node.view.data(), "undefined-identifier",
                                    "undefined constant identifier \"{}\"", id_node.spelling);
                                return;
                            }

                            constant.value_ = it->second.value_->clone();
                        }
                    });

                    if (constant.value_)
                        applySignToConstantValue(constant.value_, sc_node.sign, constant_value_location);
                },
                [&](nodes::CharacterString &cs_node) {
                    if (cs_node.value.size() == 1)
                        constant.value_.reset(new sem::ConstantValueChar(
                            cs_node.value[0]));
                    else
                        constant.value_.reset(new sem::ConstantValueString(
                            cs_node.value));
                }
            });

            if (!constant.value_) {
                // use a fallback value so that we can continue with the analysis
                constant.value_ = std::make_unique<sem::ConstantValueInteger>(0);
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
