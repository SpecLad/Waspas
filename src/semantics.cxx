module;

#include <limits>
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

sem::Block builtin_block;

struct BuiltinBlockInitializer {
    BuiltinBlockInitializer() {
        auto &c_maxint = builtin_block.constants_["maxint"];
        c_maxint.reset(new sem::ConstantInteger(
            std::numeric_limits<pascal_integer_t>::max()));

        builtin_block.types_.emplace("integer", getBuiltinTypePtr<sem::TypeInteger>());
        // TODO:
        // constants: false, true
        // types: real, boolean, char, text
        // procedures: rewrite, put, reset, get, read, write, new, dispose, pack, unpack, page
        // functions: abs, sqr, sin, cos, exp, ln, sqrt, arctan, trunc, round, ord, chr,
        //   succ, pred, odd, eof, eoln
    }

    template <typename T>
    static std::shared_ptr<const sem::Type>
    getBuiltinTypePtr() {
        return std::shared_ptr<const sem::Type>(std::shared_ptr<void>(), &T::instance());
    }
} builtin_block_init;

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
    applySignToConstant(std::shared_ptr<const sem::Constant> &v, nodes::Sign sign, const char *location) {
        if (sign == nodes::Sign::NONE) return;

        if (auto *p_integer_value = dynamic_cast<const sem::ConstantInteger *>(v.get())) {
            if (sign == nodes::Sign::MINUS) {
                if (p_integer_value->value() == std::numeric_limits<pascal_integer_t>::min()) {
                    // It should be impossible to reach this, since integer constants can only
                    // be defined with unsigned literals and negations, which can't produce
                    // the lowest integer. But just in case, we'll handle it anyway.
                    reporter_.err(location, "invalid-negation",
                        "can't negate the lowest possible integer");
                }

                v.reset(new sem::ConstantInteger(-p_integer_value->value()));
            }
        }
        else if (auto *p_real_value = dynamic_cast<const sem::ConstantReal *>(v.get())) {
            if (sign == nodes::Sign::MINUS)
                v.reset(new sem::ConstantReal(-p_real_value->value()));
        }
        else {
            reporter_.err(location, "type-mismatch",
                "a sign cannot be applied to a constant of type \"{}\"", v->typeStr());
        }
    }

    static void
    collectDefiningOccurrence(
        sem::defining_occurrences_t &dos,
        const nodes::Identifier &id_node,
        sem::DefiningOccurrence::Kind kind = sem::DefiningOccurrence::NOT_TYPE
    ) {
        // ignore duplicate IDs
        dos.try_emplace(id_node.spelling,
            sem::DefiningOccurrence{id_node.view.data(), kind});
    }

    static void
    collectDefiningOccurrencesInFieldList(
        sem::defining_occurrences_t &dos,
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
        sem::defining_occurrences_t &dos,
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
        sem::defining_occurrences_t &dos,
        const nodes::Block &block_node
    ) {
        for (auto &constant_def_node : block_node.constant_definitions)
            collectDefiningOccurrence(dos, constant_def_node.name);

        for (auto &type_def_node : block_node.type_definitions) {
            collectDefiningOccurrence(dos, type_def_node.name, sem::DefiningOccurrence::TYPE);
            collectDefiningOccurrencesInTypeDenoter(dos, *type_def_node.denoter);
        }

        for (auto &variable_decl_node : block_node.variable_declarations) {
            for (auto &identifier_node : variable_decl_node.var_names)
                collectDefiningOccurrence(dos, identifier_node);
            collectDefiningOccurrencesInTypeDenoter(dos, *variable_decl_node.var_type);
        }

        for (auto &subroutine_decl_node : block_node.subroutine_declarations) {
            // Function identifications do not introduce defining occurrences.
            // Strictly speaking, this check is unnecessary, since if a function
            // identification for a given name occurs before the corresponding
            // function heading, that's an error (and we'll catch that error later),
            // and if it occurs after, collectDefiningOccurrence will ignore it.
            // We do the check anyway, just so that we can point at the real
            // defining occurrence of the function if we need to.
            if (!dynamic_cast<nodes::FunctionIdentification *>(subroutine_decl_node.heading.get()))
                collectDefiningOccurrence(dos, subroutine_decl_node.heading->name);
        }
    }

    bool
    checkDuplicateIdentifier(
        const sem::Block &block, const nodes::Identifier &id_node
    ) {
        const auto &occurrence = block.defining_occurrences_.at(id_node.spelling);

        if (occurrence.location != id_node.view.data()) {
            reporter_.err(id_node.view.data(), "duplicate-identifier",
                "identifier \"{}\" already defined", id_node.spelling);
            reporter_.note(occurrence.location,
                "defining point of \"{}\"", id_node.spelling);
            return true;
        }

        return false;
    }

    template <typename T>
    T *
    lookupIdentifier(
        sem::Block &block,
        const nodes::Identifier &applied_occurrence_node,
        std::unordered_map<std::string, T> sem::Block::*map_member,
        std::string_view identifier_type_str
    ) {
        const auto &spelling = applied_occurrence_node.spelling;

        {
            auto &map = block.*map_member;
            auto it = map.find(spelling);
            if (it != map.end()) return &it->second;
        }

        auto applied_occurrence_location = applied_occurrence_node.view.data();
        auto &dos = block.defining_occurrences_;

        if (auto it = dos.find(spelling); it != dos.end()) {
            reporter_.err(applied_occurrence_location, "use-before-definition",
                "identifier \"{}\" used before it was defined", spelling);
            reporter_.note(it->second.location,
                "defining point of \"{}\"", spelling);
            return nullptr;
        }

        for (
            sem::Block *parent_block = block.parent_;
            parent_block;
            parent_block = parent_block->parent_
        ) {
            auto &map = parent_block->*map_member;
            auto it = map.find(spelling);
            if (it != map.end()) return &it->second;
        }

        reporter_.err(applied_occurrence_location, "undefined-identifier",
            "undefined {} identifier \"{}\"", identifier_type_str, spelling);
        return nullptr;
    }

    std::shared_ptr<const sem::Constant>
    lookupConstant(
        sem::Block &block,
        const nodes::Identifier &applied_occurrence_node
    ) {
        auto *ptr = lookupIdentifier(block, applied_occurrence_node,
            &sem::Block::constants_, "constant");
        return ptr ? *ptr : nullptr;
    }

    std::shared_ptr<const sem::Type>
    lookupType(
        sem::Block &block,
        const nodes::Identifier &applied_occurrence_node
    ) {
        auto *ptr = lookupIdentifier(block, applied_occurrence_node,
            &sem::Block::types_, "type");
        return ptr ? *ptr : nullptr;
    }

    void
    analyzeConstantDefinitions(
        const nodes::Block &block_node,
        sem::Block &block
    ) {
        for (auto &constant_def_node : block_node.constant_definitions) {
            if (checkDuplicateIdentifier(block, constant_def_node.name))
                continue;

            std::shared_ptr<const sem::Constant> constant;

            auto &constant_value_node = *constant_def_node.value;
            auto constant_value_location = constant_value_node.view.data();

            visit(constant_value_node, overloaded{
                [&, this](nodes::SignedConstant &sc_node) {
                    visit(*sc_node.unsigned_value, overloaded{
                        [&](nodes::UnsignedIntegerConstant &uic_node) {
                            constant.reset(new sem::ConstantInteger(
                                uic_node.value));
                        },
                        [&](nodes::UnsignedRealConstant &urc_node) {
                            constant.reset(new sem::ConstantReal(
                                urc_node.value));
                        },
                        [&](nodes::Identifier &id_node) {
                            constant = lookupConstant(block, id_node);
                        }
                    });

                    if (constant)
                        applySignToConstant(constant, sc_node.sign, constant_value_location);
                },
                [&](nodes::CharacterString &cs_node) {
                    if (cs_node.value.size() == 1)
                        constant.reset(new sem::ConstantChar(
                            cs_node.value[0]));
                    else
                        constant.reset(new sem::ConstantString(
                            cs_node.value));
                }
            });

            if (!constant) {
                // use a fallback value so that we can continue with the analysis
                constant = std::make_shared<sem::ConstantInteger>(0);
            }

            block.constants_.emplace(constant_def_node.name.spelling, std::move(constant));
        }
    }

    void
    analyzeTypeDefinitions(
        const nodes::Block &block_node,
        sem::Block &block
    ) {
        for (auto &type_def_node : block_node.type_definitions) {
            if (checkDuplicateIdentifier(block, type_def_node.name))
                continue;

            std::shared_ptr<const sem::Type> type;

            auto &type_denoter_node = *type_def_node.denoter;
            auto type_denoter_location = type_denoter_node.view.data();

            visit(type_denoter_node, overloaded{
                [&](nodes::NewPointerType &) {
                    reporter_.err(type_denoter_location, "unsupported-feature",
                        "pointer types are not yet supported");
                },
                [&](nodes::NewStructuredType &) {
                    reporter_.err(type_denoter_location, "unsupported-feature",
                        "structured types are not yet supported");
                },
                [&](nodes::OrdinalType &ordinal_type_node) {
                    visit(ordinal_type_node, overloaded{
                        [&](nodes::EnumeratedType &) {
                            reporter_.err(type_denoter_location, "unsupported-feature",
                                "enumerated types are not yet supported");
                        },
                        [&](nodes::Identifier &id_node) {
                            type = lookupType(block, id_node);
                        },
                        [&](nodes::SubrangeType &) {
                            reporter_.err(type_denoter_location, "unsupported-feature",
                                "subrange types are not yet supported");
                        },
                    });
                },
            });

            if (!type) {
                // use a fallback type so that we can continue with the analysis
                type = BuiltinBlockInitializer::getBuiltinTypePtr<sem::TypeInteger>();
            }
        }
    }

    void
    buildBlock(const nodes::Block &block_node, sem::Block &block) {
        analyzeLabelDeclarations(block_node, block);

        collectDefiningOccurrencesInBlock(block.defining_occurrences_, block_node);

        analyzeConstantDefinitions(block_node, block);
        analyzeTypeDefinitions(block_node, block);
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

        program.block_.parent_ = &builtin_block;
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
