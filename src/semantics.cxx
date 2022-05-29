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
    analyzeLabelDeclarations(const nodes::Block &block_node, sem::Block &block) {
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
    }

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

    struct DefiningOccurrence {
        const char *location;
        enum Kind { NOT_TYPE, TYPE } kind;
    };

    using defining_occurrences_t = std::unordered_map<std::string, DefiningOccurrence>;

    static void
    collectDefiningOccurrence(
        defining_occurrences_t &dos,
        const nodes::Identifier &id_node,
        DefiningOccurrence::Kind kind = DefiningOccurrence::NOT_TYPE
    ) {
        // ignore duplicate IDs
        dos.try_emplace(id_node.spelling, DefiningOccurrence{id_node.view.data(), kind});
    }

    static void
    collectDefiningOccurrencesInFieldList(
        defining_occurrences_t &dos,
        nodes::FieldList &field_list_node
    ) {
        for (const auto &fixed_section : field_list_node.fixed_sections)
            collectDefiningOccurrencesInTypeDenoter(dos, *fixed_section.field_type);

        if (field_list_node.variant_part)
            for (auto &variant : field_list_node.variant_part->variants)
                collectDefiningOccurrencesInFieldList(dos, variant.fields);
    }

    static void
    collectDefiningOccurrencesInTypeDenoter(
        defining_occurrences_t &dos,
        nodes::TypeDenoter &denoter_node
    ) {
        visit(denoter_node, overloaded{
            [](nodes::NewPointerType &) {},
            [&dos](nodes::NewStructuredType &structured_node) {
                visit(*structured_node.unpacked, overloaded{
                    [&dos](nodes::ArrayType &array_node) {
                        for (const auto &index_type : array_node.index_types)
                            collectDefiningOccurrencesInTypeDenoter(dos, *index_type);

                        collectDefiningOccurrencesInTypeDenoter(
                            dos, *array_node.component_type);
                    },
                    [&dos](nodes::FileType &file_node) {
                        collectDefiningOccurrencesInTypeDenoter(
                            dos, *file_node.component_type);
                    },
                    [&dos](nodes::RecordType &record_node) {
                        collectDefiningOccurrencesInFieldList(dos, record_node.fields);
                    },
                    [&dos](nodes::SetType &set_node) {
                        collectDefiningOccurrencesInTypeDenoter(
                            dos, *set_node.base_type);
                    },
                });
            },
            [&dos](nodes::OrdinalType &ordinal_node) {
                visit(ordinal_node, overloaded{
                    [&dos](nodes::EnumeratedType &enum_node) {
                        for (auto &identifier_node : enum_node.constants)
                            collectDefiningOccurrence(dos, identifier_node);
                    },
                    [](nodes::Identifier &) {},
                    [](nodes::SubrangeType &) {},
                });
            },
        });
    }

    static void
    collectDefiningOccurrencesInBlock(
        defining_occurrences_t &dos,
        const nodes::Block &block_node
    ) {
        for (auto &constant_def_node : block_node.constant_definitions)
            collectDefiningOccurrence(dos, constant_def_node.name);

        for (auto &type_def_node : block_node.type_definitions) {
            collectDefiningOccurrence(dos, type_def_node.name, DefiningOccurrence::TYPE);
            collectDefiningOccurrencesInTypeDenoter(dos, *type_def_node.denoter);
        }

        for (auto &variable_decl_node : block_node.variable_declarations) {
            for (auto &identifier_node : variable_decl_node.var_names)
                collectDefiningOccurrence(dos, identifier_node);
            collectDefiningOccurrencesInTypeDenoter(dos, *variable_decl_node.var_type);
        }

        // TODO: need to handle the distinction between subroutine headings/identifications
        //for (auto &subroutine_decl_node : block_node.subroutine_declarations)
        //    collectDefiningOccurrence(dos, subroutine_decl_node.heading->name);
    }

    bool
    checkDuplicateIdentifier(
        const defining_occurrences_t &dos, const nodes::Identifier &id_node
    ) {
        const auto &occurrence = dos.at(id_node.spelling);

        if (occurrence.location != id_node.view.data()) {
            reporter_.err(id_node.view.data(), "duplicate-identifier",
                "identifier \"{}\" already defined", id_node.spelling);
            reporter_.note(occurrence.location,
                "defining point of \"{}\"", id_node.spelling);
            return true;
        }

        return false;
    }

    void
    analyzeConstantDefinitions(
        const nodes::Block &block_node,
        const defining_occurrences_t &dos,
        sem::Block &block
    ) {
        for (auto &constant_def_node : block_node.constant_definitions) {
            if (checkDuplicateIdentifier(dos, constant_def_node.name))
                continue;

            sem::Constant constant;
            constant.location_ = constant_def_node.view.data();

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

            block.constants_.emplace(constant_def_node.name.spelling, std::move(constant));
        }
    }

    void
    buildBlock(const nodes::Block &block_node, sem::Block &block) {
        analyzeLabelDeclarations(block_node, block);

        defining_occurrences_t defining_occurrences;
        collectDefiningOccurrencesInBlock(defining_occurrences, block_node);

        analyzeConstantDefinitions(block_node, defining_occurrences, block);
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
