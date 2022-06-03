module;

#include <cassert>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>

module semantics;

using namespace std::literals;

std::string
sem::TypeSubrange::str() const {
    return smallest_value_->str() + ".."s + largest_value_->str();
}

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

sem::Block builtin_block;

struct BuiltinBlockInitializer {
    BuiltinBlockInitializer() {
        builtin_block.constants_.emplace("maxint",
            getBuiltinPtr(sem::ConstantInteger::instanceMax));

        builtin_block.constants_.emplace("false",
            getBuiltinPtr(sem::ConstantBoolean::instanceFalse));

        builtin_block.constants_.emplace("true",
            getBuiltinPtr(sem::ConstantBoolean::instanceTrue));

        for (const auto &c : builtin_block.constants_)
            builtin_block.defining_occurrences_.emplace(c.first, nullptr);

        addBuiltinTypes<
            sem::TypeBoolean, sem::TypeChar, sem::TypeInteger, sem::TypeReal,
            sem::TypeText
        >();

        for (const auto &t : builtin_block.types_)
            builtin_block.defining_occurrences_.emplace(t.first, nullptr);

        // TODO:
        // procedures: rewrite, put, reset, get, read, write, new, dispose, pack, unpack, page
        // functions: abs, sqr, sin, cos, exp, ln, sqrt, arctan, trunc, round, ord, chr,
        //   succ, pred, odd, eof, eoln
    }

    template <typename ...Ts>
    static void
    addBuiltinTypes() {
        (builtin_block.types_.emplace(Ts::NAME, getBuiltinPtr(Ts::instance)), ...);
    }

    template <typename T>
    static std::shared_ptr<T>
    getBuiltinPtr(T &(*fun)()) {
        return std::shared_ptr<T>(std::shared_ptr<void>(), &fun());
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

                v = std::make_shared<sem::ConstantInteger>(-p_integer_value->value());
            }
        }
        else if (auto *p_real_value = dynamic_cast<const sem::ConstantReal *>(v.get())) {
            if (sign == nodes::Sign::MINUS)
                v = std::make_shared<sem::ConstantReal>(-p_real_value->value());
        }
        else {
            reporter_.err(location, "type-mismatch",
                "a sign cannot be applied to a constant of type \"{}\"", v->type().str());
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
            [&dos](nodes::EnumeratedType &enum_node) {
                for (auto &identifier_node : enum_node.constants)
                    collectDefiningOccurrence(dos, identifier_node);
            },
            [](nodes::Identifier &) {},
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
            [](nodes::SubrangeType &) {},
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
        std::string_view identifier_kind_str
    ) {
        const auto &spelling = applied_occurrence_node.spelling;
        auto applied_occurrence_location = applied_occurrence_node.view.data();

        {
            auto &map = block.*map_member;

            if (auto it = map.find(spelling); it != map.end())
                return &it->second;

            auto &dos = block.defining_occurrences_;

            if (auto it = dos.find(spelling); it != dos.end()) {
                if (it->second.location > applied_occurrence_location) {
                    reporter_.err(applied_occurrence_location, "use-before-definition",
                        "identifier \"{}\" used before it was defined", spelling);
                }
                else {
                    reporter_.err(applied_occurrence_location, "wrong-identifier-kind",
                        "identifier \"{}\" is not a {} identifier",
                        spelling, identifier_kind_str);
                }
                reporter_.note(it->second.location,
                    "defining point of \"{}\"", spelling);
                return nullptr;
            }
        }

        for (
            sem::Block *parent_block = block.parent_;
            parent_block;
            parent_block = parent_block->parent_
        ) {
            auto &map = parent_block->*map_member;

            if (auto it = map.find(spelling); it != map.end())
                return &it->second;

            auto &dos = parent_block->defining_occurrences_;

            if (auto it = dos.find(spelling); it != dos.end()) {
                reporter_.err(applied_occurrence_location, "wrong-identifier-kind",
                    "identifier \"{}\" is not a {} identifier",
                    spelling, identifier_kind_str);

                // the location might be null if parent_block is the builtin block
                if (it->second.location)
                    reporter_.note(it->second.location,
                        "defining point of \"{}\"", spelling);

                return nullptr;
            }
        }

        reporter_.err(applied_occurrence_location, "undefined-identifier",
            "undefined {} identifier \"{}\"", identifier_kind_str, spelling);
        return nullptr;
    }

    std::shared_ptr<const sem::Constant> *
    lookupConstant(
        sem::Block &block,
        const nodes::Identifier &applied_occurrence_node
    ) {
        return lookupIdentifier(block, applied_occurrence_node,
            &sem::Block::constants_, "constant");
    }

    std::shared_ptr<const sem::Type> *
    lookupType(
        sem::Block &block,
        const nodes::Identifier &applied_occurrence_node
    ) {
        return lookupIdentifier(block, applied_occurrence_node,
            &sem::Block::types_, "type");
    }

    std::shared_ptr<const sem::Constant>
    resolveConstant(sem::Block &block, nodes::Constant &constant_node) {
        auto constant_location = constant_node.view.data();
        std::shared_ptr<const sem::Constant> constant;

        visit(constant_node, overloaded{
            [&, this](nodes::SignedConstant &sc_node) {
                visit(*sc_node.unsigned_value, overloaded{
                    [&](nodes::UnsignedIntegerConstant &uic_node) {
                        constant = std::make_shared<sem::ConstantInteger>(
                            uic_node.value);
                    },
                    [&](nodes::UnsignedRealConstant &urc_node) {
                        constant = std::make_shared<sem::ConstantReal>(
                            urc_node.value);
                    },
                    [&](nodes::Identifier &id_node) {
                        auto *ref_constant = lookupConstant(block, id_node);
                        if (!ref_constant) return;

                        if (!*ref_constant) {
                            reporter_.err(id_node.view.data(), "circular-definition",
                                "constant \"{}\" used in its own definition", id_node.spelling);
                            return;
                        }

                        constant = *ref_constant;
                    }
                });

                if (constant)
                    applySignToConstant(constant, sc_node.sign, constant_location);
            },
            [&](nodes::CharacterString &cs_node) {
                if (cs_node.value.size() == 1)
                    constant = std::make_shared<sem::ConstantChar>(
                        cs_node.value[0]);
                else if (cs_node.value.size() <= std::size_t(PASCAL_INTEGER_MAX))
                    constant = std::make_shared<sem::ConstantString>(
                        cs_node.value);
                else
                    reporter_.err(cs_node.view.data(), "too-many-elements",
                        "character string length ({}) greater than maxint ({})",
                        cs_node.value.size(), PASCAL_INTEGER_MAX);
            }
        });

        return constant;
    }

    void
    analyzeConstantDefinitions(
        const nodes::Block &block_node,
        sem::Block &block
    ) {
        for (auto &constant_def_node : block_node.constant_definitions) {
            if (checkDuplicateIdentifier(block, constant_def_node.name))
                continue;

            // For circular definition detection to work, we must first add
            // the new constant to block.constants_, and _then_ resolve the value.
            auto &constant = block.constants_[constant_def_node.name.spelling];
            constant = resolveConstant(block, *constant_def_node.value);

            if (!constant) {
                // use a fallback value so that we can continue with the analysis
                constant = std::make_shared<sem::ConstantInteger>(0);
            }
        }
    }

    std::shared_ptr<const sem::Type>
    resolveType(sem::Block &block, nodes::TypeDenoter &type_denoter_node) {
        auto type_denoter_location = type_denoter_node.view.data();
        std::shared_ptr<const sem::Type> type;

        visit(type_denoter_node, overloaded{
            [&](nodes::EnumeratedType &enumerated_type_node) {
                if (enumerated_type_node.constants.size()
                    > std::size_t(PASCAL_INTEGER_MAX) + 1
                ) {
                    reporter_.err(enumerated_type_node.view.data(), "too-many-elements",
                        "number of constants ({}) greater than maximum allowed ({})",
                        enumerated_type_node.constants.size(),
                        std::size_t(PASCAL_INTEGER_MAX) + 1);
                    return;
                }

                std::vector<std::string> constant_names;

                for (auto &id_node : enumerated_type_node.constants) {
                    if (checkDuplicateIdentifier(block, id_node))
                        continue;

                    constant_names.push_back(id_node.spelling);
                }

                if (constant_names.empty())
                    return;

                auto enumerated_type
                    = std::make_shared<sem::TypeEnumerated>(constant_names);

                for (const auto &constant : enumerated_type->constants())
                    block.constants_.emplace(constant->str(), constant);

                type = enumerated_type;
            },
            [&](nodes::Identifier &id_node) {
                auto *ref_type = lookupType(block, id_node);
                if (!ref_type) return;

                if (!*ref_type) {
                    reporter_.err(id_node.view.data(), "circular-definition",
                        "type \"{}\" used in its own definition", id_node.spelling);
                    return;
                }

                type = *ref_type;
            },
            [&](nodes::NewPointerType &pointer_type_node) {
                const std::string &domain_type_name
                    = pointer_type_node.domain_type.spelling;

                // Pointer types can refer to types that haven't been defined
                // yet, so we can't resolve the domain type the normal way.
                // Instead, we'll just find the block that contains the domain
                // type and store the reference to that block in the pointer type.
                // This will allow the domain type to be resolved after the block
                // is fully analyzed.
                for (const sem::Block *domain_type_block = &block;
                    domain_type_block;
                    domain_type_block = domain_type_block->parent_
                ) {
                    auto it = domain_type_block->defining_occurrences_.find(domain_type_name);

                    if (it == domain_type_block->defining_occurrences_.end())
                        continue;

                    if (it->second.kind != sem::DefiningOccurrence::TYPE) {
                        reporter_.err(pointer_type_node.domain_type.view.data(),
                            "wrong-identifier-kind",
                            "identifier \"{}\" is not a type identifier",
                            domain_type_name);

                        // the location might be null
                        // if domain_type_block is the builtin block
                        if (it->second.location)
                            reporter_.note(it->second.location,
                                "defining point of \"{}\"", domain_type_name);

                        return;
                    }

                    type = std::make_shared<sem::TypePointer>(
                        *domain_type_block, domain_type_name);
                    return;
                }

                reporter_.err(pointer_type_node.domain_type.view.data(),
                    "undefined-identifier",
                    "undefined type identifier \"{}\"", domain_type_name);
            },
            [&](nodes::NewStructuredType &structured_type_node) {
                visit(*structured_type_node.unpacked, overloaded{
                    [&](nodes::ArrayType &array_type_node) {
                        bool packed = structured_type_node.is_packed;
                        auto array_type = resolveType(block, *array_type_node.component_type);
                        if (!array_type) return;

                        for (auto &index_type_node : std::views::reverse(array_type_node.index_types)) {
                            auto index_type = resolveType(block, *index_type_node);
                            if (!index_type) return;

                            if (!index_type->isOrdinal()) {
                                reporter_.err(index_type_node->view.data(), "non-ordinal-type",
                                    "array index type is non-ordinal");
                            }

                            array_type = std::make_shared<sem::TypeArray>(index_type, array_type, packed);
                        }

                        type = array_type;
                    },
                    [&](nodes::FileType &file_type_node) {
                        auto component_type = resolveType(block, *file_type_node.component_type);
                        if (!component_type) return;

                        if (!component_type->canBeFileComponent()) {
                            reporter_.err(file_type_node.component_type->view.data(),
                                "disallowed-file-component",
                                "disallowed type used as file component");
                            return;
                        }

                        type = std::make_shared<sem::TypeFile>(
                            component_type, structured_type_node.is_packed);
                    },
                    [&](nodes::RecordType &) {
                        reporter_.err(type_denoter_location, "unsupported-feature",
                            "record types are not yet supported");
                    },
                    [&](nodes::SetType &set_type_node) {
                        auto base_type = resolveType(block, *set_type_node.base_type);
                        if (!base_type) return;

                        if (!base_type->isOrdinal()) {
                            reporter_.err(set_type_node.base_type->view.data(),
                                "non-ordinal-type", "set base type is non-ordinal");
                            return;
                        }

                        type = std::make_shared<sem::TypeSet>(
                            base_type, structured_type_node.is_packed);
                    },
                });
            },
            [&](nodes::SubrangeType &subrange_type_node) {
                auto smallest = resolveConstant(block, *subrange_type_node.smallest);
                auto largest = resolveConstant(block, *subrange_type_node.largest);

                if (!smallest || !largest) return;

                auto smallest_ordinal =
                    std::dynamic_pointer_cast<const sem::ConstantOrdinal>(smallest);

                if (!smallest_ordinal) {
                    reporter_.err(subrange_type_node.smallest->view.data(),
                        "non-ordinal-type",
                        "subrange bound has non-ordinal type \"{}\"", smallest->type().str());
                    return;
                }

                if (&largest->type() != &smallest->type()) {
                    reporter_.err(subrange_type_node.largest->view.data(),
                        "type-mismatch",
                        "largest subrange value has different type (\"{}\") "
                            "from smallest value type (\"{}\")",
                        largest->type().str(), smallest->type().str());
                    return;
                }

                auto largest_ordinal =
                    std::dynamic_pointer_cast<const sem::ConstantOrdinal>(largest);

                // Since both constants have the same type,
                // it should be impossible for largest_ordinal to be null.
                assert(largest_ordinal);

                if (largest_ordinal->ordinalNumber() < smallest_ordinal->ordinalNumber()) {
                    reporter_.err(subrange_type_node.largest->view.data(),
                        "inverted-subrange-bounds",
                        "largest subrange value is less than smallest value");
                    return;
                }

                type = std::make_shared<sem::TypeSubrange>(
                    smallest_ordinal, largest_ordinal);
            },
        });

        return type;
    }

    void
    analyzeTypeDefinitions(
        const nodes::Block &block_node,
        sem::Block &block
    ) {
        for (auto &type_def_node : block_node.type_definitions) {
            if (checkDuplicateIdentifier(block, type_def_node.name))
                continue;

            auto &type = block.types_[type_def_node.name.spelling];
            type = resolveType(block, *type_def_node.denoter);

            if (!type) {
                // use a fallback type so that we can continue with the analysis
                type = BuiltinBlockInitializer::getBuiltinPtr(sem::TypeInteger::instance);
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
