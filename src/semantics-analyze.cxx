module;

#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <ranges>
#include <unordered_set>
#include <variant>
#include <vector>

module semantics;

import utilities;

using namespace std::literals;

// In situations where a type fails to resolve this function can be used
// to recover without dropping the declaration being analysed entirely.
template <typename T>
void
applyFallback(std::shared_ptr<const T> &ptr)
    requires std::is_base_of_v<T, sem::TypeInteger>
{
    if (!ptr)
        ptr = staticPtr(sem::TypeInteger::instance());
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
                label_node.value, label_location, nullptr);

            if (!success) {
                reporter_.err(label_location, ec::DUPLICATE_LABEL,
                    "label \"{}\" already defined", label_node.value);
                reporter_.note(it->second.defining_occurrence,
                    "defining point of \"{}\"", label_node.value);
            }
        }
    }

    void
    applySignToConstant(sem::Constant::ptr_t &v, nodes::Sign sign, const char *location) {
        if (sign == nodes::Sign::NONE) return;

        if (auto *p_integer_value = dynamic_cast<const sem::ConstantInteger *>(v.get())) {
            if (sign == nodes::Sign::MINUS) {
                if (p_integer_value->value() == std::numeric_limits<pascal_integer_t>::min()) {
                    // It should be impossible to reach this, since integer constants can only
                    // be defined with unsigned literals and negations, which can't produce
                    // the lowest integer. But just in case, we'll handle it anyway.
                    reporter_.err(location, ec::INVALID_NEGATION,
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
            reporter_.err(location, ec::TYPE_MISMATCH,
                "a sign cannot be applied to a constant of type \"{}\"", v->type()->str());
        }
    }

    static void
    collectDefiningOccurrencesInFieldList(
        sem::Scope &scope,
        nodes::FieldList &field_list_node
    ) {
        for (const auto &fixed_section : field_list_node.fixed_sections)
            collectDefiningOccurrencesInTypeDenoter(scope, *fixed_section.field_type);

        if (field_list_node.variant_part)
            for (auto &variant : field_list_node.variant_part->variants)
                collectDefiningOccurrencesInFieldList(scope, variant.fields);
    }

    static void
    collectDefiningOccurrencesInTypeDenoter(
        sem::Scope &scope,
        nodes::TypeDenoter &denoter_node
    ) {
        visit(denoter_node, overloaded{
            [&scope](nodes::EnumeratedType &enum_node) {
                for (auto &identifier_node : enum_node.constants)
                    scope.add(identifier_node);
            },
            [](nodes::Identifier &) {},
            [](nodes::NewPointerType &) {},
            [&scope](nodes::NewStructuredType &structured_node) {
                visit(*structured_node.unpacked, overloaded{
                    [&scope](nodes::ArrayType &array_node) {
                        for (const auto &index_type : array_node.index_types)
                            collectDefiningOccurrencesInTypeDenoter(scope, *index_type);

                        collectDefiningOccurrencesInTypeDenoter(
                            scope, *array_node.component_type);
                    },
                    [&scope](nodes::FileType &file_node) {
                        collectDefiningOccurrencesInTypeDenoter(
                            scope, *file_node.component_type);
                    },
                    [&scope](nodes::RecordType &record_node) {
                        collectDefiningOccurrencesInFieldList(scope, record_node.fields);
                    },
                    [&scope](nodes::SetType &set_node) {
                        collectDefiningOccurrencesInTypeDenoter(
                            scope, *set_node.base_type);
                    },
                });
            },
            [](nodes::SubrangeType &) {},
        });
    }

    static void
    collectDefiningOccurrencesInBlock(
        sem::Scope &scope,
        const nodes::Block &block_node
    ) {
        for (auto &constant_def_node : block_node.constant_definitions)
            scope.add(constant_def_node.name);

        for (auto &type_def_node : block_node.type_definitions) {
            scope.add(type_def_node.name, sem::DefiningOccurrence::TYPE);
            collectDefiningOccurrencesInTypeDenoter(scope, *type_def_node.denoter);
        }

        for (auto &variable_decl_node : block_node.variable_declarations) {
            for (auto &identifier_node : variable_decl_node.var_names)
                scope.add(identifier_node);
            collectDefiningOccurrencesInTypeDenoter(scope, *variable_decl_node.var_type);
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
                scope.add(subroutine_decl_node.heading->name);
        }
    }

    bool
    checkDuplicateIdentifier(
        const sem::Scope &scope, const nodes::Identifier &id_node
    ) {
        const auto &occurrence = scope.lookupShallowUnsafe(id_node.spelling);

        if (occurrence.location != id_node.view.data()) {
            reporter_.err(id_node.view.data(), ec::DUPLICATE_IDENTIFIER,
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
        sem::Scope &scope,
        const nodes::Identifier &applied_occurrence_node,
        std::unordered_map<std::string, T> sem::Block::*map_member,
        std::string_view identifier_kind_str
    ) {
        const auto &spelling = applied_occurrence_node.spelling;
        auto applied_occurrence_location = applied_occurrence_node.view.data();

        auto generic_result = scope.lookup(spelling);
        if (!generic_result) {
            reporter_.err(applied_occurrence_location, ec::UNDEFINED_IDENTIFIER,
                "undefined {} identifier \"{}\"", identifier_kind_str, spelling);
            return nullptr;
        }

        auto *defining_scope = generic_result->scope;
        auto &defining_occurrence = generic_result->defining_occurrence;

        if (auto *block = defining_scope->block()) {
            auto &map = block->*map_member;

            if (auto it = map.find(spelling); it != map.end())
                return &it->second;
        }
        else {
            // scopes not associated with blocks aren't supposed to define types
            assert(defining_occurrence.kind == sem::DefiningOccurrence::NOT_TYPE);
        }

        if (
            // the location might be null if defining_scope is of the builtin block
            applied_occurrence_location
            && defining_occurrence.location > applied_occurrence_location
        ) {
            reporter_.err(applied_occurrence_location, ec::USE_BEFORE_DEFINITION,
                "identifier \"{}\" used before it was defined", spelling);
        }
        else {
            reporter_.err(applied_occurrence_location, ec::WRONG_IDENTIFIER_KIND,
                "identifier \"{}\" is not a {} identifier",
                spelling, identifier_kind_str);
        }

        if (defining_occurrence.location)
            reporter_.note(defining_occurrence.location,
                "defining point of \"{}\"", spelling);

        return nullptr;
    }

    sem::Constant::ptr_t *
    lookupConstant(
        sem::Scope &scope,
        const nodes::Identifier &applied_occurrence_node
    ) {
        return lookupIdentifier(scope, applied_occurrence_node,
            &sem::Block::constants_, "constant");
    }

    sem::Type::ptr_t *
    lookupType(
        sem::Scope &scope,
        const nodes::Identifier &applied_occurrence_node
    ) {
        return lookupIdentifier(scope, applied_occurrence_node,
            &sem::Block::types_, "type");
    }

    sem::Constant::ptr_t
    resolveSignableConstant(
        sem::Scope &scope, nodes::UnsignedIntegerConstant &uic_node
    ) {
        return std::make_shared<sem::ConstantInteger>(uic_node.value);
    }

    sem::Constant::ptr_t
    resolveSignableConstant(
        sem::Scope &scope, nodes::UnsignedRealConstant &urc_node
    ) {
        return std::make_shared<sem::ConstantReal>(urc_node.value);
    }

    sem::Constant::ptr_t
    resolveSignableConstant(
        sem::Scope &scope, nodes::Identifier &id_node
    ) {
        auto *ref_constant = lookupConstant(scope, id_node);
        if (!ref_constant) return nullptr;

        if (!*ref_constant) {
            reporter_.err(id_node.view.data(), ec::CIRCULAR_DEFINITION,
                "constant \"{}\" used in its own definition", id_node.spelling);
            return nullptr;
        }

        return *ref_constant;
    }

    sem::Constant::ptr_t
    resolveConstant(sem::Scope &scope, nodes::Constant &constant_node) {
        auto constant_location = constant_node.view.data();
        sem::Constant::ptr_t constant;

        visit(constant_node, overloaded{
            [&, this](nodes::SignedConstant &sc_node) {
                constant = visit(*sc_node.unsigned_value,
                    [&](auto &signable_constant_node) {
                        return resolveSignableConstant(scope, signable_constant_node);
                    }
                );

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
                    reporter_.err(cs_node.view.data(), ec::TOO_MANY_ELEMENTS,
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
            if (checkDuplicateIdentifier(block.scope_, constant_def_node.name))
                continue;

            // For circular definition detection to work, we must first add
            // the new constant to block.constants_, and _then_ resolve the value.
            auto &constant = block.constants_[constant_def_node.name.spelling];
            constant = resolveConstant(block.scope_, *constant_def_node.value);

            if (!constant) {
                // use a fallback value so that we can continue with the analysis
                constant = std::make_shared<sem::ConstantInteger>(0);
            }
        }
    }

    std::shared_ptr<const sem::TypeArray>
    resolveStructuredType(
        sem::Scope &scope, nodes::ArrayType &array_type_node, bool is_packed
    ) {
        auto component_type = resolveType(scope, *array_type_node.component_type);
        if (!component_type) return nullptr;

        std::shared_ptr<const sem::TypeArray> array_type;

        for (auto &index_type_node : std::views::reverse(array_type_node.index_types)) {
            auto index_type = resolveType(scope, *index_type_node);
            if (!index_type) return nullptr;

            auto index_type_ordinal
                = std::dynamic_pointer_cast<const sem::TypeOrdinal>(index_type);

            if (!index_type_ordinal) {
                reporter_.err(index_type_node->view.data(), ec::NON_ORDINAL_TYPE,
                    "array index type is non-ordinal");
                return nullptr;
            }

            array_type = std::make_shared<sem::TypeArray>(
                index_type_ordinal, component_type, is_packed);
            component_type = array_type;
        }

        // The grammar requires at least one index type, so the loop should
        // execute at least once.
        assert(array_type);

        return array_type;
    }

    std::shared_ptr<const sem::TypeFile>
    resolveStructuredType(
        sem::Scope &scope, nodes::FileType &file_type_node, bool is_packed
    ) {
        auto component_type = resolveType(scope, *file_type_node.component_type);
        if (!component_type) return nullptr;

        if (!component_type->canBeFileComponent()) {
            reporter_.err(file_type_node.component_type->view.data(),
                ec::DISALLOWED_FILE_COMPONENT,
                "disallowed type used as file component");
            return nullptr;
        }

        return std::make_shared<sem::TypeFile>(component_type, is_packed);
    }

    static void
    collectFieldDefiningOccurrences(
        sem::Scope &scope,
        nodes::FieldList &field_list_node
    ) {
        // Not to be confused with collectOccurrencesInFieldList -
        // that collects identifiers scoped to the block, this collects
        // field names.

        for (const auto &fixed_section : field_list_node.fixed_sections)
            for (const auto &field_name : fixed_section.field_names)
                scope.add(field_name);

        if (auto &variant_part = field_list_node.variant_part) {
            if (variant_part->tag_field)
                scope.add(*variant_part->tag_field);

            for (auto &variant : variant_part->variants)
                collectFieldDefiningOccurrences(scope, variant.fields);
        }
    }

    sem::FieldList
    resolveFieldList(
        sem::Scope &scope,
        nodes::FieldList &field_list_node
    ) {
        sem::FieldList field_list;

        for (const auto &fixed_section : field_list_node.fixed_sections) {
            auto type = resolveType(scope, *fixed_section.field_type);
            if (!type) continue;

            for (const auto &field_name : fixed_section.field_names) {
                if (checkDuplicateIdentifier(scope, field_name))
                    continue;

                field_list.addField(field_name.spelling, type);
            }
        }

        if (auto &variant_part_node = field_list_node.variant_part) {
            auto tag_type_node = variant_part_node->tag_type;
            auto tag_type = resolveTypeDenoter(scope, tag_type_node);
            if (!tag_type) return field_list;

            auto tag_type_ordinal
                = std::dynamic_pointer_cast<const sem::TypeOrdinal>(tag_type);
            if (!tag_type_ordinal) {
                reporter_.err(tag_type_node.view.data(), ec::NON_ORDINAL_TYPE,
                    "variant part tag type is not ordinal");
                return field_list;
            }

            sem::VariantPart variant_part(tag_type_ordinal);

            if (auto &tag_field_node = variant_part_node->tag_field) {
                if (checkDuplicateIdentifier(scope, *tag_field_node))
                    return field_list;

                variant_part.setTagField(tag_field_node->spelling);
            }

            pascal_integer_t tag_smallest_ordinal = tag_type_ordinal->smallestOrdinal();
            pascal_integer_t tag_largest_ordinal = tag_type_ordinal->largestOrdinal();

            pascal_integer_t counter = tag_smallest_ordinal;
            bool counter_overflowed = false;

            std::unordered_map<pascal_integer_t, const char *> used_ordinals;

            for (auto &variant : variant_part_node->variants) {
                std::vector<sem::ConstantOrdinal::ptr_t> case_constants;

                for (auto &constant_node : variant.case_constants) {
                    auto constant = resolveConstant(scope, *constant_node);
                    if (!constant) return field_list;

                    if (!constant->type()->isCompatibleWith(*tag_type_ordinal)) {
                        reporter_.err(constant_node->view.data(), ec::TYPE_MISMATCH,
                            "case constant type (\"{}\") is incompatible with tag type (\"{}\")",
                            constant->type()->str(), tag_type_ordinal->str());
                        return field_list;
                    }

                    auto ordinal_constant
                        = std::dynamic_pointer_cast<const sem::ConstantOrdinal>(constant);
                    assert(ordinal_constant); // the type check above guarantees this

                    auto ordinal = ordinal_constant->ordinalNumber();
                    if (!(tag_smallest_ordinal <= ordinal && ordinal <= tag_largest_ordinal)) {
                        reporter_.err(constant_node->view.data(), ec::OUT_OF_RANGE,
                            "case constant is not within the range of values of the tag type");
                        return field_list;
                    }

                    if (auto it = used_ordinals.find(ordinal); it != used_ordinals.end()) {
                        reporter_.err(constant_node->view.data(), ec::DUPLICATE_CASE,
                            "case constant already used");
                        reporter_.note(it->second, "previous occurrence of the case constant");
                        return field_list;
                    }
                    used_ordinals.insert_or_assign(ordinal, constant_node->view.data());

                    case_constants.push_back(ordinal_constant);

                    if (counter == PASCAL_INTEGER_MAX) {
                        // Note that we can only reach this branch once, because
                        // if the counter reached PASCAL_INTEGER_MAX, then every
                        // constant between tag_smallest_ordinal and tag_largest_ordinal
                        // has already been used, so if there are any constants left,
                        // they are either invalid or duplicates, so we'll error
                        // out before reaching here again.
                        assert(!counter_overflowed);
                        counter_overflowed = true;
                    }
                    else {
                        ++counter;
                    }
                }

                auto variant_fields = resolveFieldList(scope, variant.fields);

                variant_part.addVariant(case_constants, variant_fields);
            }

            // This could only be false if there were no case constants,
            // which the grammar isn't supposed to allow.
            assert(counter_overflowed || counter != tag_smallest_ordinal);

            if (tag_largest_ordinal != (counter_overflowed ? counter : counter - 1)) {
                reporter_.err(variant_part_node->view.data(), ec::MISSING_CASE,
                    "at least one value of the tag type is not covered by a case constant");
                return field_list;
            }

            field_list.setVariantPart(variant_part);
        }

        return field_list;
    }

    std::shared_ptr<const sem::TypeRecord>
    resolveStructuredType(
        sem::Scope &scope, nodes::RecordType &record_type_node, bool is_packed
    ) {
        sem::Scope record_scope(&scope);
        collectFieldDefiningOccurrences(record_scope, record_type_node.fields);

        return std::make_shared<sem::TypeRecord>(
            resolveFieldList(record_scope, record_type_node.fields),
            is_packed);
    }

    std::shared_ptr<const sem::TypeSet>
    resolveStructuredType(
        sem::Scope &scope, nodes::SetType &set_type_node, bool is_packed
    ) {
        auto base_type = resolveType(scope, *set_type_node.base_type);
        if (!base_type) return nullptr;

        auto base_type_ordinal
            = std::dynamic_pointer_cast<const sem::TypeOrdinal>(base_type);
        if (!base_type_ordinal) {
            reporter_.err(set_type_node.base_type->view.data(),
                ec::NON_ORDINAL_TYPE, "set base type is non-ordinal");
            return nullptr;
        }

        return std::make_shared<sem::TypeSet>(base_type_ordinal, is_packed);
    }

    std::shared_ptr<const sem::TypeEnumerated>
    resolveTypeDenoter(
        sem::Scope &scope, nodes::EnumeratedType &enumerated_type_node
    ) {
        if (enumerated_type_node.constants.size()
            > std::size_t(PASCAL_INTEGER_MAX) + 1
        ) {
            reporter_.err(enumerated_type_node.view.data(), ec::TOO_MANY_ELEMENTS,
                "number of constants ({}) greater than maximum allowed ({})",
                enumerated_type_node.constants.size(),
                std::size_t(PASCAL_INTEGER_MAX) + 1);
            return nullptr;
        }

        std::vector<std::string> constant_names;

        for (auto &id_node : enumerated_type_node.constants) {
            if (checkDuplicateIdentifier(scope, id_node))
                continue;

            constant_names.push_back(id_node.spelling);
        }

        if (constant_names.empty())
            return nullptr;

        auto enumerated_type = sem::TypeEnumerated::make(constant_names);

        for (const auto &constant : enumerated_type->constants())
            scope.closestContainingBlock().constants_.emplace(constant->str(), constant);

        return enumerated_type;
    }

    sem::Type::ptr_t
    resolveTypeDenoter(
        sem::Scope &scope, nodes::Identifier &id_node
    ) {
        auto *ref_type = lookupType(scope, id_node);
        if (!ref_type) return nullptr;

        if (!*ref_type) {
            reporter_.err(id_node.view.data(), ec::CIRCULAR_DEFINITION,
                "type \"{}\" used in its own definition", id_node.spelling);
            return nullptr;
        }

        return *ref_type;
    }

    std::shared_ptr<const sem::TypePointer>
    resolveTypeDenoter(
        sem::Scope &scope, nodes::NewPointerType &pointer_type_node
    ) {
        const std::string &domain_type_name
            = pointer_type_node.domain_type.spelling;

        // Pointer types can refer to types that haven't been defined
        // yet, so we can't resolve the domain type the normal way.
        // Instead, we'll just find the block that contains the domain
        // type and store the reference to that block in the pointer type.
        // This will allow the domain type to be resolved after the block
        // is fully analyzed.

        auto lookup_result = scope.lookup(domain_type_name);
        if (!lookup_result) {
            reporter_.err(pointer_type_node.domain_type.view.data(),
                ec::UNDEFINED_IDENTIFIER,
                "undefined type identifier \"{}\"", domain_type_name);
            return nullptr;
        }

        auto *defining_scope = lookup_result->scope;
        auto &defining_occurrence = lookup_result->defining_occurrence;

        if (defining_occurrence.kind != sem::DefiningOccurrence::TYPE) {
            reporter_.err(pointer_type_node.domain_type.view.data(),
                ec::WRONG_IDENTIFIER_KIND,
                "identifier \"{}\" is not a type identifier",
                domain_type_name);

            // the location might be null
            // if the scope is for the builtin block
            if (defining_occurrence.location)
                reporter_.note(defining_occurrence.location,
                    "defining point of \"{}\"", domain_type_name);

            return nullptr;
        }

        assert(defining_scope->block());

        return std::make_shared<sem::TypePointer>(
            *defining_scope->block(), domain_type_name);
    }

    sem::Type::ptr_t
    resolveTypeDenoter(
        sem::Scope &scope, nodes::NewStructuredType &structured_type_node
    ) {
        return visit(*structured_type_node.unpacked,
            [&](auto &node) -> sem::Type::ptr_t {
                return resolveStructuredType(scope, node, structured_type_node.is_packed);
            }
        );
    }

    std::shared_ptr<const sem::TypeSubrange>
    resolveTypeDenoter(
        sem::Scope &scope, nodes::SubrangeType &subrange_type_node
    ) {
        auto smallest = resolveConstant(scope, *subrange_type_node.smallest);
        auto largest = resolveConstant(scope, *subrange_type_node.largest);

        if (!smallest || !largest) return nullptr;

        auto smallest_ordinal =
            std::dynamic_pointer_cast<const sem::ConstantOrdinal>(smallest);

        if (!smallest_ordinal) {
            reporter_.err(subrange_type_node.smallest->view.data(),
                ec::NON_ORDINAL_TYPE,
                "subrange bound has non-ordinal type \"{}\"", smallest->type()->str());
            return nullptr;
        }

        if (largest->type() != smallest->type()) {
            reporter_.err(subrange_type_node.largest->view.data(),
                ec::TYPE_MISMATCH,
                "largest subrange value has different type (\"{}\") "
                    "from smallest value type (\"{}\")",
                largest->type()->str(), smallest->type()->str());
            return nullptr;
        }

        auto largest_ordinal =
            std::dynamic_pointer_cast<const sem::ConstantOrdinal>(largest);

        // Since both constants have the same type,
        // it should be impossible for largest_ordinal to be null.
        assert(largest_ordinal);

        if (largest_ordinal->ordinalNumber() < smallest_ordinal->ordinalNumber()) {
            reporter_.err(subrange_type_node.largest->view.data(),
                ec::INVERTED_SUBRANGE_BOUNDS,
                "largest subrange value is less than smallest value");
            return nullptr;
        }

        return std::make_shared<sem::TypeSubrange>(smallest_ordinal, largest_ordinal);
    }

    sem::Type::ptr_t
    resolveType(sem::Scope &scope, nodes::TypeDenoter &type_denoter_node) {
        return visit(type_denoter_node,
            [&](auto &node) -> sem::Type::ptr_t {
                return resolveTypeDenoter(scope, node);
            }
        );
    }

    void
    analyzeTypeDefinitions(
        const nodes::Block &block_node,
        sem::Block &block
    ) {
        for (auto &type_def_node : block_node.type_definitions) {
            if (checkDuplicateIdentifier(block.scope_, type_def_node.name))
                continue;

            auto &type = block.types_[type_def_node.name.spelling];
            type = resolveType(block.scope_, *type_def_node.denoter);
            applyFallback(type);
        }
    }

    void
    analyzeVariableDeclarations(
        const nodes::Block &block_node,
        sem::Block &block
    ) {
        for (auto &var_decl_node : block_node.variable_declarations) {
            auto type = resolveType(block.scope_, *var_decl_node.var_type);
            applyFallback(type);

            for (auto &var_name_node : var_decl_node.var_names) {
                if (checkDuplicateIdentifier(block.scope_, var_name_node))
                    continue;

                block.variables_.try_emplace(var_name_node.spelling, type);
            }
        }
    }

    static void
    collectBoundDefiningOccurrences(
        sem::Scope &scope,
        nodes::FormalParameterTypeOrSchema &type_or_schema_node
    ) {
        visit(type_or_schema_node, overloaded{
            [&](const nodes::ConformantArraySchema &schema_node) {
                for (auto &index_type_node : schema_node.index_types) {
                    scope.add(index_type_node.smallest);
                    scope.add(index_type_node.largest);
                }

                collectBoundDefiningOccurrences(scope, *schema_node.component_type);
            },
            [](const nodes::Identifier &) {},
        });
    }

    sem::DynamicType::ptr_t
    resolveDynamicType(
        sem::Scope &scope, nodes::FormalParameterTypeOrSchema &type_or_schema_node
    ) {
        return visit(type_or_schema_node, overloaded{
            [&](nodes::ConformantArraySchema &schema_node)
                -> sem::DynamicType::ptr_t
            {
                auto component_type = resolveDynamicType(scope, *schema_node.component_type);
                if (!component_type) return nullptr;

                std::shared_ptr<const sem::ConformantArraySchema> schema;

                for (auto &index_type_node : std::views::reverse(schema_node.index_types)) {
                    if (checkDuplicateIdentifier(scope, index_type_node.smallest))
                        return nullptr;

                    if (checkDuplicateIdentifier(scope, index_type_node.largest))
                        return nullptr;

                    auto bound_type = resolveType(scope, index_type_node.bound_type);
                    if (!bound_type) return nullptr;

                    auto bound_type_ordinal
                        = std::dynamic_pointer_cast<const sem::TypeOrdinal>(bound_type);

                    if (!bound_type_ordinal) {
                        reporter_.err(index_type_node.bound_type.view.data(), ec::NON_ORDINAL_TYPE,
                            "bound type is non-ordinal");
                        return nullptr;
                    }

                    schema = std::make_shared<sem::ConformantArraySchema>(
                        index_type_node.smallest.spelling, index_type_node.largest.spelling,
                        bound_type_ordinal, component_type, schema_node.is_packed);
                    component_type = schema;
                }

                // The grammar requires at least one index type, so the loop should
                // execute at least once.
                assert(schema);

                return schema;
            },
            [&](nodes::Identifier &id_node) -> sem::DynamicType::ptr_t {
                return resolveTypeDenoter(scope, id_node);
            },
        });
    }

    struct SignatureWithScope {
        sem::Signature signature;
        sem::Scope scope;
    };

    SignatureWithScope
    resolveSignature(
        sem::Scope &scope,
        std::span<std::unique_ptr<nodes::FormalParameterSection>> parameter_section_nodes,
        nodes::Identifier *result_type_node
    ) {
        sem::Scope parameter_list_scope(&scope);
        std::vector<sem::FormalParameterSection> parameters;

        for (auto &parameter_section_node : parameter_section_nodes) {
            visit(*parameter_section_node, overloaded{
                [&](nodes::SubroutineHeading &heading_node) {
                    parameter_list_scope.add(heading_node.name);
                },
                [&](nodes::RegularParameterSection &rps_node) {
                    for (auto &id_node : rps_node.parameter_names)
                        parameter_list_scope.add(id_node);

                    collectBoundDefiningOccurrences(
                        parameter_list_scope, *rps_node.parameter_type);
                },
            });
        }

        for (auto &parameter_section_node : parameter_section_nodes) {
            visit(*parameter_section_node, overloaded{
                [&](nodes::FunctionHeading &heading_node) {
                    if (checkDuplicateIdentifier(parameter_list_scope, heading_node.name))
                        return;

                    auto sws = resolveSignature(
                        parameter_list_scope,
                        heading_node.parameters, &heading_node.result_type);

                    parameters.push_back(sem::FormalParameterSection{
                        sem::SubroutineParameterSpecification(
                            heading_node.name.spelling, sws.signature)});
                },
                [&](nodes::ProcedureHeading &heading_node) {
                    if (checkDuplicateIdentifier(parameter_list_scope, heading_node.name))
                        return;

                    auto sws = resolveSignature(
                        parameter_list_scope,
                        heading_node.parameters, nullptr);

                    parameters.push_back(sem::FormalParameterSection{
                        sem::SubroutineParameterSpecification(
                            heading_node.name.spelling, sws.signature)});
                },
                [&](nodes::RegularParameterSection &rps_node) {
                    std::vector<std::string> names;

                    for (auto &id_node : rps_node.parameter_names) {
                        if (checkDuplicateIdentifier(parameter_list_scope, id_node))
                            continue;

                        names.push_back(id_node.spelling);
                    }

                    if (names.empty()) return;

                    auto type = resolveDynamicType(parameter_list_scope, *rps_node.parameter_type);

                    if (type && !rps_node.is_variable && !type->canBeFileComponent()) {
                        reporter_.err(rps_node.parameter_type->view.data(),
                            ec::DISALLOWED_PARAMETER_TYPE,
                            "disallowed type \"{}\" used as value parameter type",
                            type->str());
                        type = nullptr;
                    }

                    applyFallback(type);

                    parameters.push_back(sem::FormalParameterSection{
                        sem::RegularParameterSection(
                            rps_node.is_variable, names, type)});
                },
            });
        }

        sem::Type::ptr_t result_type;

        if (result_type_node) {
            if (auto type = resolveTypeDenoter(scope, *result_type_node)) {
                if (dynamic_cast<const sem::TypeOrdinal *>(type.get())
                    || dynamic_cast<const sem::TypeReal *>(type.get())
                    || dynamic_cast<const sem::TypePointer *>(type.get())
                ) {
                    result_type = type;
                }
                else {
                    reporter_.err(result_type_node->view.data(),
                        ec::DISALLOWED_RESULT_TYPE,
                        "result type \"{}\" is neither a simple nor a pointer type",
                        type->str());
                }
            }

            // If result_type is nullptr, we can't just leave it as that,
            // since that would turn the function into a procedure
            // and cause more errors down the line.
            applyFallback(result_type);
        }

        return {
            sem::Signature(parameters, result_type),
            parameter_list_scope,
        };
    }

    using label_set_t = std::unordered_set<pascal_integer_t>;

    void
    analyzeSubroutineDeclarations(
        const nodes::Block &block_node,
        sem::Block &block,
        const label_set_t &allowed_goto_targets
    ) {
        std::unordered_set<std::string> forward_declarations;

        for (auto &subr_decl_node : block_node.subroutine_declarations) {
            const auto &subr_name_node = subr_decl_node.heading->name;
            const auto &subr_name = subr_name_node.spelling;

            enum SubroutineType { PROCEDURE = 0, FUNCTION = 1 };
            static constexpr std::string_view SUBROUTINE_TYPE_STRS[] = {"procedure"sv, "function"sv};
            using optional_signature_t = std::variant<SignatureWithScope, SubroutineType>;

            optional_signature_t opt_sig = visit(
                *subr_decl_node.heading, overloaded{
                    [&](nodes::FunctionHeading &function_head_node) {
                        return optional_signature_t(
                            resolveSignature(
                                block.scope_,
                                function_head_node.parameters,
                                &function_head_node.result_type
                            )
                        );
                    },
                    [&](nodes::FunctionIdentification &function_id_node) {
                        return optional_signature_t(FUNCTION);
                    },
                    [&](nodes::ProcedureHeading &procedure_head_node) {
                        bool is_delayed = forward_declarations.contains(subr_name)
                            && procedure_head_node.parameters.empty();

                        if (is_delayed) return optional_signature_t(PROCEDURE);

                        return optional_signature_t(
                            resolveSignature(
                                block.scope_,
                                procedure_head_node.parameters,
                                nullptr
                            )
                        );
                    },
                }
            );

            sem::Subroutine *subroutine = std::visit(overloaded{
                [&](const SignatureWithScope &sws) -> sem::Subroutine * {
                    // this is the first declaration of this subroutine

                    if (checkDuplicateIdentifier(block.scope_, subr_name_node))
                        return nullptr;

                    auto [it, success] = block.subroutines_.try_emplace(
                        subr_name, subr_name_node.view.data(), sws.signature, block);

                    it->second.block_.scope_.mergeFrom(sws.scope);
                    return &it->second;
                },
                [&](SubroutineType subr_type) -> sem::Subroutine * {
                    // this is a delayed declaration of this subroutine

                    auto it = block.subroutines_.find(subr_name);

                    if (!forward_declarations.contains(subr_name)) {
                        if (it == block.subroutines_.end()) {
                            reporter_.err(subr_name_node.view.data(),
                                ec::MISSING_FORWARD_DECLARATION,
                                "delayed declaration with no preceding forward declaration");
                        }
                        else {
                            reporter_.err(subr_name_node.view.data(),
                                ec::DUPLICATE_SUBROUTINE_DECLARATION,
                                "duplicate declaration for \"{}\"", subr_name);
                            reporter_.note(it->second.last_declaration_location_,
                                "last declaration of \"{}\"", subr_name);
                        }

                        return nullptr;
                    }

                    assert(it != block.subroutines_.end());
                    auto &previous_subroutine = it->second;
                    SubroutineType previous_subroutine_type
                        = previous_subroutine.signature().resultType() ? FUNCTION : PROCEDURE;

                    if (previous_subroutine_type != subr_type) {
                        reporter_.err(subr_name_node.view.data(),
                            ec::MISMATCHED_SUBROUTINE_DECLARATION,
                            "\"{}\" declared as a {} when it had previously been declared as a {}",
                            subr_name,
                            SUBROUTINE_TYPE_STRS[subr_type],
                            SUBROUTINE_TYPE_STRS[previous_subroutine_type]);
                        reporter_.note(previous_subroutine.last_declaration_location_,
                            "last declaration of \"{}\"", subr_name);
                        return nullptr;
                    }

                    previous_subroutine.last_declaration_location_ = subr_name_node.view.data();
                    forward_declarations.erase(subr_name);
                    return &previous_subroutine;
                },
            }, opt_sig);

            if (!subroutine) continue;

            if (subr_decl_node.block) {
                buildBlock(
                    *subr_decl_node.block, subroutine->block_, allowed_goto_targets);

                if (
                    subroutine->signature_.resultType()
                    && !subroutine->contains_result_assignment_
                )
                    reporter_.err(subr_decl_node.block->view.data(),
                        ec::MISSING_RESULT_ASSIGNMENT,
                        "function block does not contain an assignment"
                            " to the function identifier \"{}\"",
                        subr_name);
            }
            else {
                forward_declarations.insert(subr_name);
            }
        }

        std::vector<const char *> missing_declaration_locations;
        for (const auto &name : forward_declarations)
            missing_declaration_locations.push_back(
                block.scope_.lookupShallowUnsafe(name).location);

        std::ranges::sort(missing_declaration_locations);

        for (const char *error_location : missing_declaration_locations)
            reporter_.err(error_location, ec::MISSING_DELAYED_DECLARATION,
                "forward declaration with no following delayed declaration");
    }

    static std::unique_ptr<sem::VariableAccess>
    resolveVariableOrFdIdentifier(
        sem::Scope &, sem::Scope::LookupResult &lr, const std::string &name
    ) {
        if (auto *block = lr.scope->block()) {
            if (block->variables_.contains(name)) {
                return std::make_unique<sem::VariableAccessVariableId>(
                    name, lr.scope_index);
            }

            if (auto *subroutine = block->containingSubroutine()) {
                if (subroutine->signature().hasRegularParameter(name))
                    return std::make_unique<sem::VariableAccessParameterId>(
                        name, lr.scope_index);
            }

            return nullptr;
        }

        if (auto *with = lr.scope->statementWith())
            if (with->variableType().fieldList().hasField(name))
                return std::make_unique<sem::VariableAccessFieldDesignatorId>(
                    name, lr.scope_index);

        return nullptr;
    }

    static std::unique_ptr<sem::Expression>
    resolveVariableFdConstantOrBoundIdentifier(
        sem::Scope &scope, sem::Scope::LookupResult &lr, const std::string &name
    ) {
        if (auto access = resolveVariableOrFdIdentifier(scope, lr, name))
            return access;

        if (auto *block = lr.scope->block()) {
            if (auto it = block->constants_.find(name); it != block->constants_.end())
                return std::make_unique<sem::ExpressionConstant>(it->second);

            if (auto *subroutine = block->containingSubroutine()) {
                if (subroutine->signature().hasBound(name))
                    return std::make_unique<sem::ExpressionBound>(name, lr.scope_index);
            }
        }

        return nullptr;
    }

    static std::unique_ptr<sem::VariableAccess>
    resolveVariableFdOrCurrentFunctionIdentifier(
        sem::Scope &scope, sem::Scope::LookupResult &lr, const std::string &name
    ) {
        if (auto access = resolveVariableOrFdIdentifier(scope, lr, name))
            return access;

        if (lr.scope_index == 0) return nullptr;

        if (auto *block = lr.scope->block()) {
            auto it = block->subroutines_.find(name);
            if (it == block->subroutines_.end()) return nullptr;

            auto &subroutine = it->second;
            if (!subroutine.signature().resultType()) return nullptr;

            // Check that `scope` is within the function's scope
            // (IOW, that the function is a _current_ function).
            if (&scope.parent(lr.scope_index - 1) != &subroutine.block().scope())
                return nullptr;

            return std::make_unique<sem::VariableAccessActivationResult>(
                name, lr.scope_index);
        }

        return nullptr;
    }

    std::unique_ptr<sem::Expression>
    resolveIndex(
        sem::Scope &scope,
        const sem::TypeOrdinal &expected_index_type,
        const nodes::Expression &index_expression_node
    ) {
        auto index = resolveExpression(scope, index_expression_node);
        const auto &index_type = index->type(scope);
        const auto &index_type_promoted = index_type.promoted();

        if (!index_type_promoted.isAssignmentCompatibleWith(expected_index_type)) {
            reporter_.err(index_expression_node.view.data(),
                ec::TYPE_MISMATCH,
                "index expression type \"{}\" is not assignment-compatible"
                    " with the array index type \"{}\"",
                index_type_promoted.str(), expected_index_type.str());
            return nullptr;
        }

        return index;
    }

    void
    applyComponentAccess(
        sem::Scope &scope,
        std::unique_ptr<sem::VariableAccess> &access,
        const std::vector<std::unique_ptr<nodes::VariableModifier>> &modifier_nodes
    ) {
        for (auto &modifier_node : modifier_nodes) {
            const auto &access_type = access->type(scope);

            visit(*modifier_node, overloaded{
                [&](nodes::DereferencingModifier &deref_mod_node) {
                    if (dynamic_cast<const sem::TypePointer *>(&access_type)) {
                        access = std::make_unique<sem::VariableAccessDereference>(
                            std::move(access));
                        return;
                    }

                    if (dynamic_cast<const sem::TypeFileLike *>(&access_type)) {
                        access = std::make_unique<sem::VariableAccessBuffer>(
                            std::move(access));
                        return;
                    }

                    reporter_.err(deref_mod_node.view.data(),
                        ec::TYPE_MISMATCH,
                        "dereferenced value of non-pointer, non-file type \"{}\"",
                        access_type.str());
                },
                [&](nodes::FieldAccessModifier &field_mod_node) {
                    auto *record_type = dynamic_cast<const sem::TypeRecord *>(
                        &access_type);

                    if (!record_type) {
                        reporter_.err(field_mod_node.view.data(),
                            ec::NON_RECORD_TYPE,
                            "accessing a field of a value of a non-record type \"{}\"",
                            access_type.str());
                        return;
                    }

                    const auto &field_name = field_mod_node.field.spelling;

                    if (!record_type->fieldList().hasField(field_name)) {
                        reporter_.err(field_mod_node.field.view.data(),
                            ec::UNDEFINED_IDENTIFIER,
                            "type \"{}\" has no field named \"{}\"",
                            record_type->str(), field_name);
                        return;
                    }

                    access = std::make_unique<sem::VariableAccessField>(
                        std::move(access), field_mod_node.field.spelling);
                },
                [&](nodes::IndexingModifier &indexing_mod_node) {
                    auto *current_access_type = &access_type;

                    for (auto &index_node : indexing_mod_node.indices) {
                        sem::TypeOrdinal::ptr_t expected_index_type;

                        if (
                            auto *array_type
                                = dynamic_cast<const sem::TypeArray *>(current_access_type)
                        ) {
                            auto index = resolveIndex(scope, *array_type->indexType(), index_node);
                            if (!index) return;
                            access = std::make_unique<sem::VariableAccessIndexed>(
                                std::move(access), std::move(index));
                        }
                        else if (
                            auto *schema
                                = dynamic_cast<const sem::ConformantArraySchema *>(current_access_type)
                        ) {
                            auto index = resolveIndex(scope, *schema->boundType(), index_node);
                            if (!index) return;
                            access = std::make_unique<sem::VariableAccessIndexedDynamic>(
                                std::move(access), std::move(index));
                        }
                        else {
                            reporter_.err(index_node.view.data(),
                                ec::NON_ARRAY_TYPE,
                                "indexing a value of a non-array type \"{}\"",
                                current_access_type->str());
                            return;
                        }

                        current_access_type = &access->type(scope);
                    }
                },
            });
        }
    }

    template <typename T>
    std::unique_ptr<T>
    resolveVariableAccessLike(
        sem::Scope &scope,
        const nodes::VariableAccess &access_node,
        std::unique_ptr<T> (*resolve_identifier)(
            sem::Scope &, sem::Scope::LookupResult &, const std::string &),
        std::string_view identifier_kind_str
    ) requires std::is_base_of_v<sem::Expression, T> {
        const auto &name = access_node.variable.spelling;

        auto lookup_result = scope.lookup(name);

        if (!lookup_result) {
            reporter_.err(access_node.variable.view.data(), ec::UNDEFINED_IDENTIFIER,
                "undefined identifier \"{}\"", name);
            return nullptr;
        }

        auto value = resolve_identifier(scope, *lookup_result, name);

        if (!value) {
            reporter_.err(access_node.variable.view.data(), ec::WRONG_IDENTIFIER_KIND,
                "identifier \"{}\" is not a {} identifier", name, identifier_kind_str);
            return nullptr;
        }

        if (!access_node.modifiers.empty()) {
            if (auto access_raw = dynamic_cast<sem::VariableAccess *>(value.get()))
                if (!dynamic_cast<sem::VariableAccessActivationResult *>(access_raw))
                {
                    std::unique_ptr<sem::VariableAccess> access(access_raw);
                    value.release();

                    applyComponentAccess(scope, access, access_node.modifiers);
                    return access;
                }

            reporter_.err(access_node.modifiers[0]->view.data(),
                ec::INVALID_COMPONENT_ACCESS,
                "accessing component of a value of an identifier"
                    " that is not a variable access");
            return nullptr;
        }

        return value;
    }

    std::unique_ptr<sem::VariableAccess>
    resolveVariableAccess(
        sem::Scope &scope, const nodes::VariableAccess &access_node
    ) {
        return resolveVariableAccessLike(scope, access_node,
            resolveVariableOrFdIdentifier, "variable or field designator");
    }

    std::unique_ptr<sem::Expression>
    resolveFactor(
        sem::Scope &scope,
        nodes::CharacterString &character_string_node
    ) {
        auto constant = resolveConstant(scope, character_string_node);

        // apply fallback
        if (!constant)
            constant = std::make_shared<sem::ConstantString>("???");

        return std::make_unique<sem::ExpressionConstant>(constant);
    }

    std::unique_ptr<sem::Expression>
    resolveFactor(sem::Scope &, nodes::Nil &nil_node) {
        return std::make_unique<sem::ExpressionNil>();
    }

    std::unique_ptr<sem::Expression>
    resolveFactor(sem::Scope &scope, nodes::Parenthetical &parenthetical_node) {
        return resolveExpression(scope, parenthetical_node.inner_expression);
    }

    template <typename T>
    std::unique_ptr<sem::Expression>
    resolveFactor(
        sem::Scope &scope,
        T &signable_constant_node
    ) requires std::is_base_of_v<nodes::SignableConstant, T> {
        auto constant = resolveSignableConstant(scope, signable_constant_node);

        // apply fallback
        if (!constant)
            constant = std::make_shared<sem::ConstantInteger>(0);

        return std::make_unique<sem::ExpressionConstant>(constant);
    }

    std::unique_ptr<sem::Expression>
    resolveFactor(
        sem::Scope &scope,
        nodes::VariableAccess &variable_access_node
    ) {
        // TODO: support function identifiers
        auto access = resolveVariableAccessLike(
            scope, variable_access_node,
            resolveVariableFdConstantOrBoundIdentifier,
            "variable, field designator, constant or bound");

        // apply fallback
        if (!access)
            access = std::make_unique<sem::ExpressionConstant>(
                std::make_shared<sem::ConstantInteger>(0));

        return access;
    }

    std::unique_ptr<sem::Expression>
    resolveFactor(
        sem::Scope &scope,
        auto &factor_node
    ) {
        reporter_.err(factor_node.view.data(), ec::UNSUPPORTED_FEATURE,
            "factor type not supported");
        return std::make_unique<sem::ExpressionConstant>(
            std::make_shared<sem::ConstantInteger>(0));
    }

    std::unique_ptr<sem::Expression>
    resolveTerm(
        sem::Scope &scope,
        const nodes::Term &term_node
    ) {
        auto expression = visit(*term_node.operand,
            [&](auto &factor_node) {
                return resolveFactor(scope, factor_node);
            }
        );

        if (!term_node.modifiers.empty())
            reporter_.err(term_node.modifiers[0].view.data(),
                ec::UNSUPPORTED_FEATURE,
                "operators are not supported");

        return expression;
    }

    std::unique_ptr<sem::Expression>
    resolveSimpleExpression(
        sem::Scope &scope,
        const nodes::SimpleExpression &simple_expression_node
    ) {
        if (simple_expression_node.sign != nodes::Sign::NONE)
            reporter_.err(simple_expression_node.view.data(),
                ec::UNSUPPORTED_FEATURE,
                "operators are not supported");

        auto expression = resolveTerm(scope, simple_expression_node.operand);

        if (!simple_expression_node.modifiers.empty())
            reporter_.err(simple_expression_node.modifiers[0].view.data(),
                ec::UNSUPPORTED_FEATURE,
                "operators are not supported");

        return expression;
    }

    std::unique_ptr<sem::Expression>
    resolveExpression(
        sem::Scope &scope,
        const nodes::Expression &expression_node
    ) {
        auto expression = resolveSimpleExpression(scope, expression_node.operand);

        if (expression_node.modifier)
            reporter_.err(expression_node.modifier->view.data(),
                ec::UNSUPPORTED_FEATURE,
                "operators are not supported");

        return expression;
    }

    struct ControlVariable {
        std::string name;
        const char *location;
    };

    struct StatementAnalysisContext {
        StatementAnalysisContext(
            linked_list_ptr_t<label_set_t> allowed_goto_targets,
            linked_list_ptr_t<ControlVariable> used_control_variables
        )
            : allowed_goto_targets(allowed_goto_targets)
            , used_control_variables(used_control_variables)
        {}

        StatementAnalysisContext
        withNewAllowedGotoTargets(linked_list_ptr_t<label_set_t> new_targets) const {
            return StatementAnalysisContext(new_targets, used_control_variables);
        }

        StatementAnalysisContext
        withNewUsedControlVariable(linked_list_ptr_t<ControlVariable> new_variable) const {
            return StatementAnalysisContext(allowed_goto_targets, new_variable);
        }

        linked_list_ptr_t<label_set_t> allowed_goto_targets;
        linked_list_ptr_t<ControlVariable> used_control_variables;
    };

    static
    std::unique_ptr<sem::Statement>
    fallbackStatement() {
        return std::make_unique<sem::StatementEmpty>();
    }

    void
    threatenVariable(
        sem::Scope &scope,
        const sem::VariableAccess &access,
        const char *location,
        linked_list_ptr_t<ControlVariable> used_control_variables
    ) {
        auto *variable_id_access
            = dynamic_cast<const sem::VariableAccessVariableId *>(&access);
        if (!variable_id_access) return;

        auto &closest_block = scope.closestContainingBlock();
        auto &variable_block
            = *scope.parent(variable_id_access->scopeIndex()).block();

        if (&variable_block == &closest_block) {
            for (const auto &used_control_variable : used_control_variables)
                if (variable_id_access->id() == used_control_variable.name) {
                    reporter_.err(location,
                        ec::THREATENED_CONTROL_VARIABLE,
                        "variable access threatens a control variable of a for loop");
                    reporter_.note(used_control_variable.location,
                        "use of \"{}\" as a control variable",
                        used_control_variable.name);
                    // This error doesn't interfere with analysis,
                    // so we don't need to report it to the caller.
                    break;
                }
        }
        else {
            auto &var = variable_block.variables_.at(variable_id_access->id());
            if (!var.subroutine_threat_location)
                var.subroutine_threat_location = location;
        }
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Scope &scope,
        const nodes::AssignmentStatement &assignment_statement_node,
        const StatementAnalysisContext &context
    ) {
        auto access = resolveVariableAccessLike(scope, assignment_statement_node.access,
            &resolveVariableFdOrCurrentFunctionIdentifier,
            "variable, field designator or current function");
        if (!access)
            return fallbackStatement();
        const auto &access_type = access->type(scope);

        threatenVariable(
            scope,
            *access, assignment_statement_node.access.view.data(),
            context.used_control_variables);

        auto expression = resolveExpression(scope, assignment_statement_node.expression);
        const auto &expression_type = expression->type(scope);
        const auto &expression_type_promoted = expression_type.promoted();

        if (!expression_type_promoted.isAssignmentCompatibleWith(access_type)) {
            reporter_.err(assignment_statement_node.expression.view.data(),
                ec::TYPE_MISMATCH,
                "right-hand side expression type \"{}\" is assignment-incompatible"
                    " with left-hand side type \"{}\"",
                expression_type_promoted.str(), access_type.str());
            return fallbackStatement();
        }

        if (
            auto *result_access
                = dynamic_cast<sem::VariableAccessActivationResult *>(access.get())
        ) {
            auto *defining_block = scope.parent(result_access->scopeIndex()).block();
            assert(defining_block);
            auto &function = defining_block->subroutine(result_access->id());
            function.contains_result_assignment_ = true;
        }

        return std::make_unique<sem::StatementAssignment>(
            std::move(access), std::move(expression));
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Scope &scope,
        const nodes::CaseStatement &case_statement_node,
        const StatementAnalysisContext &context
    ) {
        auto case_index = resolveExpression(scope, case_statement_node.case_index);
        const auto &case_index_type = case_index->type(scope);
        const auto &case_index_type_promoted = case_index_type.promoted();

        if (!dynamic_cast<const sem::TypeOrdinal *>(&case_index_type_promoted)) {
            reporter_.err(case_statement_node.case_index.view.data(),
                ec::NON_ORDINAL_TYPE,
                "case index has non-ordinal type \"{}\"",
                case_index_type_promoted.str());

            return fallbackStatement();
        }

        std::vector<sem::CaseListElement> case_list_elements;

        std::unordered_map<pascal_integer_t, const char *> used_ordinals;

        for (auto &element_node : case_statement_node.cases) {
            std::vector<sem::ConstantOrdinal::ptr_t> case_constants;

            for (auto &constant_node : element_node.constants) {
                auto constant = resolveConstant(scope, *constant_node);
                if (!constant) continue;

                auto constant_type = constant->type();

                if (constant_type.get() != &case_index_type_promoted) {
                    reporter_.err(constant_node->view.data(), ec::TYPE_MISMATCH,
                        "case constant has type \"{}\","
                            " which is different from the type of the case index (\"{}\")",
                        constant_type->str(), case_index_type_promoted.str());
                    continue;
                }

                auto ordinal_constant
                    = std::dynamic_pointer_cast<const sem::ConstantOrdinal>(constant);
                assert(ordinal_constant); // the type check above guarantees this

                auto ordinal = ordinal_constant->ordinalNumber();

                if (auto it = used_ordinals.find(ordinal); it != used_ordinals.end()) {
                    reporter_.err(constant_node->view.data(), ec::DUPLICATE_CASE,
                        "case constant already used");
                    reporter_.note(it->second, "previous occurrence of the case constant");
                    continue;
                }
                used_ordinals.insert_or_assign(ordinal, constant_node->view.data());

                case_constants.push_back(ordinal_constant);
            }

            auto statement = resolveStatement(scope, element_node.statement, context);

            if (!case_constants.empty())
                case_list_elements.emplace_back(case_constants, std::move(statement));
        }

        if (case_list_elements.empty())
            return fallbackStatement();

        return std::make_unique<sem::StatementCase>(std::move(case_list_elements));
    }

    std::unique_ptr<sem::StatementCompound>
    resolveUnlabeledStatement(
        sem::Scope &scope,
        const nodes::CompoundStatement &compound_statement_node,
        const StatementAnalysisContext &context
    ) {
        std::vector<std::unique_ptr<sem::Statement>> statements;
        LinkedListNode<label_set_t> new_allowed_targets(context.allowed_goto_targets);

        for (auto &statement_node : compound_statement_node.statements)
            if (statement_node.label)
                new_allowed_targets.value.insert(statement_node.label->value);

        const auto &new_context = new_allowed_targets.value.empty()
            ? context : context.withNewAllowedGotoTargets(&new_allowed_targets);

        for (auto &statement_node : compound_statement_node.statements) {
            statements.push_back(resolveStatement(scope, statement_node, new_context));
        }

        return std::make_unique<sem::StatementCompound>(std::move(statements));
    }

    std::unique_ptr<sem::StatementEmpty>
    resolveUnlabeledStatement(
        sem::Scope &, const nodes::EmptyStatement &, const StatementAnalysisContext &
    ) {
        return std::make_unique<sem::StatementEmpty>();
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Scope &scope,
        const nodes::ForStatement &for_statement_node,
        const StatementAnalysisContext &context
    ) {
        LinkedListNode<ControlVariable> control_variable(
            context.used_control_variables,
            for_statement_node.control_variable.spelling,
            for_statement_node.control_variable.view.data());

        sem::Scope *lookup_scope = &scope;
        std::size_t scope_index = 0;
        while (!lookup_scope->block()) {
            if (lookup_scope->containsShallow(control_variable.value.name)) {
                reporter_.err(control_variable.value.location,
                    ec::WRONG_IDENTIFIER_KIND,
                    "identifier \"{}\" is not a variable identifier",
                    control_variable.value.name);
                return fallbackStatement();
            }

            lookup_scope = lookup_scope->parent();
            assert(lookup_scope);
            ++scope_index;
        }

        auto &block = *lookup_scope->block();
        auto it = block.variables_.find(control_variable.value.name);

        if (it == block.variables_.end()) {
            reporter_.err(control_variable.value.location,
                ec::UNDEFINED_IDENTIFIER,
                "undefined variable identifier");
            return fallbackStatement();
        }

        auto control_variable_type
            = std::dynamic_pointer_cast<const sem::TypeOrdinal>(it->second.type);

        if (!control_variable_type) {
            reporter_.err(control_variable.value.location,
                ec::NON_ORDINAL_TYPE,
                "control variable has non-ordinal type \"{}\"", it->second.type->str());
            return fallbackStatement();
        }

        if (it->second.subroutine_threat_location) {
            reporter_.err(control_variable.value.location,
                ec::THREATENED_CONTROL_VARIABLE,
                "control variable is threatened by a statement in a procedure or function");
            reporter_.note(it->second.subroutine_threat_location,
                "location of threat");
            // This error doesn't interfere with analysis, so we won't abort here.
        }

        threatenVariable(scope,
            sem::VariableAccessVariableId(control_variable.value.name, scope_index),
            control_variable.value.location,
            context.used_control_variables);

        auto initial_value = resolveExpression(scope, for_statement_node.initial_value);
        const auto &initial_value_type = initial_value->type(scope);
        if (!initial_value_type.promoted().isCompatibleWith(*control_variable_type)) {
            reporter_.err(for_statement_node.initial_value.view.data(),
                ec::TYPE_MISMATCH,
                "initial value type \"{}\" is incompatible"
                    " with the control variable type \"{}\"",
                initial_value_type.promoted().str(), control_variable_type->str());
            return fallbackStatement();
        }

        auto final_value = resolveExpression(scope, for_statement_node.final_value);
        const auto &final_value_type = final_value->type(scope);

        if (!final_value_type.promoted().isCompatibleWith(*control_variable_type)) {
            reporter_.err(for_statement_node.final_value.view.data(),
                ec::TYPE_MISMATCH,
                "final value type \"{}\" is incompatible"
                    " with the control variable type \"{}\"",
                final_value_type.promoted().str(), control_variable_type->str());
            return fallbackStatement();
        }

        return std::make_unique<sem::StatementFor>(
            control_variable.value.name,
            std::move(initial_value),
            for_statement_node.direction,
            std::move(final_value),
            resolveStatement(scope, for_statement_node.body,
                context.withNewUsedControlVariable(&control_variable)));
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Scope &scope,
        const nodes::GotoStatement &goto_statement_node,
        const StatementAnalysisContext &context
    ) {
        pascal_integer_t label = goto_statement_node.label.value;
        std::size_t scope_index = 0;

        for (
            sem::Scope *lookup_scope = &scope;
            lookup_scope;
            lookup_scope = lookup_scope->parent(), ++scope_index
        ) {
            if (sem::Block *block = lookup_scope->block()) {
                auto it = block->labels_.find(label);
                if (it != block->labels_.end()) {
                    for (const auto &allowed_targets : context.allowed_goto_targets)
                        if (allowed_targets.contains(label))
                            return std::make_unique<sem::StatementGoto>(label, scope_index);

                    reporter_.err(goto_statement_node.label.view.data(),
                        ec::DISALLOWED_GOTO_TARGET,
                        "disallowed target label for this goto statement");
                    return fallbackStatement();
                }
            }
        }

        reporter_.err(goto_statement_node.label.view.data(),
            ec::UNDEFINED_LABEL, "undefined label");
        return fallbackStatement();
    }

    std::unique_ptr<sem::Expression>
    resolveCondition(sem::Scope &scope, const nodes::Expression &expression_node) {
        auto condition = resolveExpression(scope, expression_node);
        const auto &condition_type = condition->type(scope);
        const auto &condition_type_promoted = condition_type.promoted();
        if (&condition_type_promoted != &sem::TypeBoolean::instance()) {
            reporter_.err(expression_node.view.data(),
                ec::NON_BOOLEAN_TYPE,
                "condition has non-boolean type \"{}\"", condition_type_promoted.str());

            return std::make_unique<sem::ExpressionConstant>(
                staticPtr(sem::ConstantBoolean::instanceFalse()));
        }

        return condition;
    }

    std::unique_ptr<sem::StatementIf>
    resolveUnlabeledStatement(
        sem::Scope &scope,
        const nodes::IfStatement &if_statement_node,
        const StatementAnalysisContext &context
    ) {
        // These variables shouldn't be inlined in the `make_unique` call,
        // because we need to make sure that the true branch is resolved before
        // the false branch (and thus the error messages from it are emitted first).
        auto condition = resolveCondition(scope, if_statement_node.condition);
        auto true_branch = resolveStatement(
            scope, if_statement_node.true_branch, context);
        auto false_branch = if_statement_node.false_branch
            ? resolveStatement(scope, *if_statement_node.false_branch, context)
            : nullptr;

        return std::make_unique<sem::StatementIf>(
            std::move(condition),
            std::move(true_branch), std::move(false_branch));
    }

    void
    checkNoFormattingSpecification(const nodes::ActualParameter &parameter_node) {
        if (parameter_node.formatting_specification)
            reporter_.err(parameter_node.formatting_specification->view.data(),
                ec::DISALLOWED_PARAMETER_FORM,
                "unexpected total width specification");
    }

    bool
    checkActualParameterIsConformable(
        const sem::DynamicType &expression_type,
        const char *expression_location,
        const sem::ConformantArraySchema &schema,
        const sem::DynamicType *first_good_parameter_type,
        const char *first_good_parameter_location
    ) {
        if (!first_good_parameter_type) {
            if (!expression_type.isConformableWith(schema)) {
                reporter_.err(expression_location,
                    ec::TYPE_MISMATCH,
                    "type of actual parameter (\"{}\") is not conformable "
                    "with schema of formal parameter (\"{}\")",
                    expression_type.str(), schema.str());
                return false;
            }
        }
        else {
            if (&expression_type != first_good_parameter_type) {
                reporter_.err(expression_location,
                    ec::TYPE_MISMATCH,
                    "type of actual parameter (\"{}\") is different "
                    "from type of a previous parameter (\"{}\")",
                    expression_type.str(), first_good_parameter_type->str());
                reporter_.note(first_good_parameter_location,
                    "location of previous parameter");
                return false;
            }
        }

        return true;
    }

    const nodes::VariableAccess *
    checkExpressionIsVariableAccess(
        const nodes::Expression &expression_node,
        std::string_view expected_construct_str
    ) {
        if (expression_node.modifier) {
            reporter_.err(expression_node.modifier->view.data(),
                ec::DISALLOWED_PARAMETER_FORM,
                "operator in {}", expected_construct_str);
            return nullptr;
        }

        auto &simple_expression_node = expression_node.operand;

        if (simple_expression_node.sign != nodes::Sign::NONE) {
            reporter_.err(simple_expression_node.view.data(),
                ec::DISALLOWED_PARAMETER_FORM,
                "operator in {}", expected_construct_str);
            return nullptr;
        }

        if (!simple_expression_node.modifiers.empty()) {
            reporter_.err(simple_expression_node.modifiers[0].view.data(),
                ec::DISALLOWED_PARAMETER_FORM,
                "operator in {}", expected_construct_str);
            return nullptr;
        }

        auto &term_node = simple_expression_node.operand;

        if (!term_node.modifiers.empty()) {
            reporter_.err(term_node.modifiers[0].view.data(),
                ec::DISALLOWED_PARAMETER_FORM,
                "operator in {}", expected_construct_str);
            return nullptr;
        }

        auto *variable_access_node = dynamic_cast<const nodes::VariableAccess *>(
            term_node.operand.get());

        if (!variable_access_node) {
            reporter_.err(term_node.operand->view.data(),
                ec::DISALLOWED_PARAMETER_FORM,
                "not a {}", expected_construct_str);
            return nullptr;
        }

        return variable_access_node;
    }

    std::unique_ptr<sem::VariableAccess>
    resolveExpressionAsVariableAccess(
        sem::Scope &scope, const nodes::Expression &expression_node
    ) {
        auto *variable_access_node = checkExpressionIsVariableAccess(
            expression_node, "variable access");

        if (!variable_access_node)
            return nullptr;

        return resolveVariableAccess(scope, *variable_access_node);
    }

    std::optional<sem::actual_parameter_section_t>
    resolveActualParameterSection(
        sem::Scope &scope,
        const sem::RegularParameterSection &rps,
        std::vector<nodes::ActualParameter>::const_iterator &actual_parameter_it,
        const std::vector<nodes::ActualParameter>::const_iterator &actual_parameter_end,
        const char *actual_parameter_end_location,
        const StatementAnalysisContext &context
    ) {
        if (actual_parameter_end - actual_parameter_it < rps.names().size()) {
            reporter_.err(actual_parameter_end_location,
                ec::PARAMETER_COUNT_MISMATCH,
                "no actual parameter corresponding to formal parameter \"{}\"",
                rps.names()[actual_parameter_end - actual_parameter_it]);
            return std::nullopt;
        }

        const sem::DynamicType *first_good_parameter_type = nullptr;
        const char *first_good_parameter_location = nullptr;

        auto rps_schema
            = std::dynamic_pointer_cast<const sem::ConformantArraySchema>(
                rps.type());

        std::optional<sem::actual_parameter_section_t> result;

        if (rps.isVariable()) {
            std::vector<std::unique_ptr<sem::VariableAccess>> accesses;

            for (const auto &parameter_node
                : std::views::counted(actual_parameter_it, rps.names().size())
            ) {
                checkNoFormattingSpecification(parameter_node);

                auto access = resolveExpressionAsVariableAccess(
                    scope, parameter_node.value);
                if (!access) continue;
                const auto &access_type = access->type(scope);

                if (rps_schema) {
                    if (!checkActualParameterIsConformable(
                        access_type, parameter_node.value.view.data(),
                        *rps_schema,
                        first_good_parameter_type, first_good_parameter_location
                    ))
                        continue;
                }
                else {
                    if (&access_type != rps.type().get()) {
                        reporter_.err(parameter_node.value.view.data(),
                            ec::TYPE_MISMATCH,
                            "type of actual parameter (\"{}\") is different"
                                " from type of formal parameter (\"{}\")",
                            access_type.str(), rps.type()->str());
                        continue;
                    }
                }

                if (auto *field_access
                    = dynamic_cast<sem::VariableAccessField *>(access.get())
                ) {
                    const auto &record_type = dynamic_cast<const sem::TypeRecord &>(
                        field_access->record().type(scope));

                    if (record_type.isPacked()) {
                        reporter_.err(parameter_node.value.view.data(),
                            ec::DISALLOWED_PARAMETER_FORM,
                            "field of packed record used as a variable parameter");
                        continue;
                    }

                    if (record_type.fieldList().fieldIsTag(field_access->fieldName())) {
                        reporter_.err(parameter_node.value.view.data(),
                            ec::DISALLOWED_PARAMETER_FORM,
                            "tag field used as a variable parameter");
                        continue;
                    }
                }
                else if (auto *indexed_access
                    = dynamic_cast<sem::VariableAccessIndexed *>(access.get())
                ) {
                    const auto &array_type = dynamic_cast<const sem::TypeArray &>(
                        indexed_access->array().type(scope));

                    if (array_type.isPacked()) {
                        reporter_.err(parameter_node.value.view.data(),
                            ec::DISALLOWED_PARAMETER_FORM,
                            "component of packed array used as a variable parameter");
                        continue;
                    }
                }
                else if (auto *indexed_access
                    = dynamic_cast<sem::VariableAccessIndexedDynamic *>(access.get())
                ) {
                    const auto &schema = dynamic_cast<const sem::ConformantArraySchema &>(
                        indexed_access->dynamicArray().type(scope));

                    if (schema.isPacked()) {
                        reporter_.err(parameter_node.value.view.data(),
                            ec::DISALLOWED_PARAMETER_FORM,
                            "component of packed array used as a variable parameter");
                        continue;
                    }
                }

                threatenVariable(
                    scope,
                    *access, parameter_node.value.view.data(),
                    context.used_control_variables);

                if (accesses.empty()) {
                    first_good_parameter_type = &access_type;
                    first_good_parameter_location = parameter_node.view.data();
                }
                accesses.push_back(std::move(access));
            }

            if (accesses.size() == rps.names().size())
                result.emplace(std::move(accesses));
        }
        else {
            std::vector<std::unique_ptr<sem::Expression>> expressions;

            for (const auto &parameter_node
                : std::views::counted(actual_parameter_it, rps.names().size())
            ) {
                checkNoFormattingSpecification(parameter_node);

                auto expression = resolveExpression(scope, parameter_node.value);
                const auto &expression_type = expression->type(scope);

                if (rps_schema) {
                    if (!checkActualParameterIsConformable(
                        expression_type, parameter_node.value.view.data(),
                        *rps_schema,
                        first_good_parameter_type, first_good_parameter_location
                    ))
                        continue;

                    if (dynamic_cast<const sem::ConformantArraySchema *>(&expression_type)) {
                        reporter_.err(parameter_node.value.view.data(),
                            ec::DISALLOWED_PARAMETER_FORM,
                            "conformant array used directly as a value parameter");
                        continue;
                    }
                }
                else {
                    const auto &expression_type_promoted = expression_type.promoted();
                    if (!expression_type_promoted.isAssignmentCompatibleWith(*rps.type())) {
                        reporter_.err(parameter_node.value.view.data(),
                            ec::TYPE_MISMATCH,
                            "type of actual parameter (\"{}\") is assignment-incompatible"
                                " with type of formal parameter (\"{}\")",
                            expression_type_promoted.str(), rps.type()->str());
                        continue;
                    }
                }

                if (expressions.empty()) {
                    first_good_parameter_type = &expression_type;
                    first_good_parameter_location = parameter_node.view.data();
                }
                expressions.push_back(std::move(expression));
            }

            if (expressions.size() == rps.names().size())
                result.emplace(std::move(expressions));
        }

        actual_parameter_it += rps.names().size();

        return result;
    }

    std::optional<std::pair<sem::SubroutineReference, const sem::Signature *>>
    resolveExpressionAsSubroutineReference(
        sem::Scope &scope,
        const nodes::Expression &expression_node,
        bool need_function
    ) {
        std::string_view expected_construct_str = need_function
            ? "function identifier"sv : "procedure identifier"sv;

        auto *variable_access_node = checkExpressionIsVariableAccess(
            expression_node, expected_construct_str);

        if (!variable_access_node) return std::nullopt;

        if (!variable_access_node->modifiers.empty()) {
            reporter_.err(variable_access_node->modifiers[0]->view.data(),
                ec::DISALLOWED_PARAMETER_FORM,
                "accessing a component of a {}", expected_construct_str);
            return std::nullopt;
        }

        using result_t
            = std::optional<std::pair<sem::SubroutineReference, const sem::Signature *>>;

        return std::visit(
            overloaded{
                [](const std::monostate &) { return result_t{}; },
                [&](const BuiltinMarker &) {
                    reporter_.err(variable_access_node->variable.view.data(),
                        ec::DISALLOWED_PARAMETER_FORM,
                        "builtin {} passed as parameter", expected_construct_str);
                    return result_t{};
                },
                [](const std::pair<sem::SubroutineReference, const sem::Signature *> &p) {
                    return result_t(p);
                },
            },
            lookupSubroutineReference(
                scope, variable_access_node->variable, need_function)
        );
    }

    std::optional<sem::actual_parameter_section_t>
    resolveActualParameterSection(
        sem::Scope &scope,
        const sem::SubroutineParameterSpecification &sps,
        std::vector<nodes::ActualParameter>::const_iterator &actual_parameter_it,
        const std::vector<nodes::ActualParameter>::const_iterator &actual_parameter_end,
        const char *actual_parameter_end_location,
        const StatementAnalysisContext &
    ) {
        if (actual_parameter_it == actual_parameter_end) {
            reporter_.err(actual_parameter_end_location,
                ec::PARAMETER_COUNT_MISMATCH,
                "no actual parameter corresponding to formal parameter \"{}\"",
                sps.name());
            return std::nullopt;
        }

        auto &parameter_node = *actual_parameter_it++;
        checkNoFormattingSpecification(parameter_node);

        auto lookup_result = resolveExpressionAsSubroutineReference(
            scope, parameter_node.value, bool(sps.signature().resultType()));

        if (!lookup_result)
            return std::nullopt;

        auto &[ref, signature] = *lookup_result;

        if (!signature->isCongruousWith(sps.signature())) {
            reporter_.err(parameter_node.value.view.data(),
                ec::TYPE_MISMATCH,
                "formal parameter list of the actual parameter "
                    "is incongruous with that of the formal parameter");
            return std::nullopt;
        }

        if (signature->resultType() != sps.signature().resultType()) {
            reporter_.err(parameter_node.value.view.data(),
                ec::TYPE_MISMATCH,
                "result type of the actual parameter "
                "is different from that of the formal parameter");
            return std::nullopt;
        }

        return sem::actual_parameter_section_t(ref);
    }

    std::vector<sem::actual_parameter_section_t>
    resolveActualParameters(
        sem::Scope &scope,
        const sem::Signature &signature,
        const std::vector<nodes::ActualParameter> &actual_parameter_nodes,
        const char *actual_parameter_end_location,
        const StatementAnalysisContext &context
    ) {
        std::vector<sem::actual_parameter_section_t> actual_parameters;

        auto node_it = actual_parameter_nodes.begin();

        for (const auto &parameter_section : signature.parameters()) {
            auto node_it_prev = node_it;

            auto actual_parameter_section = std::visit([&](const auto &s) {
                return resolveActualParameterSection(
                    scope, s, node_it, actual_parameter_nodes.end(),
                    actual_parameter_end_location,
                    context);
            }, parameter_section.v);

            // This indicates that there weren't enough actual parameters
            // to resolve the section, so we don't need to continue.
            if (node_it_prev == node_it)
                return actual_parameters;

            if (actual_parameter_section)
                actual_parameters.push_back(std::move(*actual_parameter_section));
        }

        if (node_it != actual_parameter_nodes.end())
            reporter_.err(node_it->view.data(),
                ec::PARAMETER_COUNT_MISMATCH, "extraneous actual parameter");

        return actual_parameters;
    }

    using builtin_procedure_call_f
        = std::unique_ptr<sem::Statement>(ProgramBuilder:: *)(
            sem::Scope &scope,
            std::span<const nodes::ActualParameter> actual_parameter_nodes,
            const char *actual_parameter_end_location,
            const StatementAnalysisContext &context
        );

    static const std::unordered_map<std::string_view, builtin_procedure_call_f>
        BUILTIN_PROCEDURES;

    struct BuiltinMarker {};

    std::variant<
        std::monostate, // not found / wrong kind
        BuiltinMarker, // builtin
        std::pair<sem::SubroutineReference, const sem::Signature *> // defined
    >
    lookupSubroutineReference(
        sem::Scope &scope, const nodes::Identifier &id_node, bool need_function
    ) {
        const auto &id = id_node.spelling;
        auto lookup_result = scope.lookup(id);

        const sem::Signature *signature = nullptr;
        sem::SubroutineReference::Kind kind;

        if (lookup_result) {
            if (auto *block = lookup_result->scope->block())
                if (block->hasSubroutine(id)) {
                    signature = &block->subroutine(id).signature();
                    kind = sem::SubroutineReference::REGULAR;
                }
                else if (auto *container = block->containingSubroutine()) {
                    if (container->signature().hasSubroutineParameter(id)) {
                        signature = &container->signature().subroutineParameterSignature(id);
                        kind = sem::SubroutineReference::PARAMETER;
                    }
                }
        }
        else {
            if (BUILTIN_PROCEDURES.contains(id)) {
                if (!need_function) return BuiltinMarker{};
            }
            // TODO: handle builtin functions
            else {
                reporter_.err(id_node.view.data(),
                    ec::UNDEFINED_IDENTIFIER,
                    "undefined procedure identifier \"{}\"", id);
                return std::monostate{};
            }
        }

        if (!signature || bool(signature->resultType()) != need_function) {
            reporter_.err(id_node.view.data(),
                ec::WRONG_IDENTIFIER_KIND,
                "identifier \"{}\" is not a {} identifier",
                id, need_function ? "function" : "procedure");
            return std::monostate{};
        }

        return std::make_pair(
            sem::SubroutineReference(id, lookup_result->scope_index, kind),
            signature
        );
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Scope &scope,
        const nodes::ProcedureStatement &procedure_statement_node,
        const StatementAnalysisContext &context
    ) {
        const std::string &procedure_name = procedure_statement_node.procedure.spelling;

        const char *actual_parameter_end_location
            = procedure_statement_node.parameters.empty()
                ? procedure_statement_node.procedure.view.data()
                    + procedure_statement_node.procedure.view.size()
                : procedure_statement_node.parameters.back().view.data()
                    + procedure_statement_node.parameters.back().view.size();

        return std::visit(
            overloaded{
                [](const std::monostate &) -> std::unique_ptr<sem::Statement> {
                    return fallbackStatement();
                },
                [&](const BuiltinMarker &) {
                    return (this->*BUILTIN_PROCEDURES.at(procedure_name))(
                        scope, procedure_statement_node.parameters,
                        actual_parameter_end_location, context);
                },
                [&](const std::pair<sem::SubroutineReference, const sem::Signature *> &p)
                    -> std::unique_ptr<sem::Statement>
                {
                    auto &[ref, signature] = p;

                    auto actual_parameters = resolveActualParameters(
                        scope, *signature, procedure_statement_node.parameters,
                        actual_parameter_end_location, context);

                    if (actual_parameters.size() != signature->parameters().size())
                        return fallbackStatement();

                    return std::make_unique<sem::StatementProcedure>(
                        ref, std::move(actual_parameters));
                },
            },
            lookupSubroutineReference(
                scope, procedure_statement_node.procedure, false)
        );
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Scope &scope,
        const nodes::RepeatStatement &repeat_statement_node,
        const StatementAnalysisContext &context
    ) {
        std::vector<std::unique_ptr<sem::Statement>> statements;
        LinkedListNode<label_set_t> new_allowed_targets(context.allowed_goto_targets);

        for (auto &statement_node : repeat_statement_node.statements)
            if (statement_node.label)
                new_allowed_targets.value.insert(statement_node.label->value);

        const auto &new_context = new_allowed_targets.value.empty()
            ? context : context.withNewAllowedGotoTargets(&new_allowed_targets);

        for (auto &statement_node : repeat_statement_node.statements) {
            statements.push_back(resolveStatement(scope, statement_node, new_context));
        }

        auto condition = resolveCondition(scope, repeat_statement_node.condition);

        return std::make_unique<sem::StatementRepeat>(
            std::move(statements), std::move(condition));
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Scope &scope,
        const nodes::WhileStatement &while_statement_node,
        const StatementAnalysisContext &context
    ) {
        auto condition = resolveCondition(scope, while_statement_node.condition);
        auto body = resolveStatement(scope, while_statement_node.body, context);

        return std::make_unique<sem::StatementWhile>(
            std::move(condition), std::move(body));
    }

    std::unique_ptr<sem::Statement>
    resolveWithStatementHelper(
        sem::Scope &scope,
        const nodes::WithStatement &with_statement_node,
        std::size_t variable_index,
        const StatementAnalysisContext &context
    ) {
        if (variable_index == with_statement_node.variables.size())
            return resolveStatement(scope, with_statement_node.body, context);

        auto &variable_node = with_statement_node.variables[variable_index];
        auto variable = resolveVariableAccess(scope, variable_node);
        if (!variable)
            return fallbackStatement();

        const auto &variable_type = variable->type(scope);
        auto *record_type = dynamic_cast<const sem::TypeRecord *>(
            &variable_type);
        if (!record_type) {
            reporter_.err(variable_node.view.data(),
                ec::NON_RECORD_TYPE,
                "variable has non-record type \"{}\"", variable_type.str());
            return fallbackStatement();
        }

        auto with_statement = std::make_unique<sem::StatementWith>(
            scope, std::move(variable));

        auto &with_scope = with_statement->scope();

        for (const auto &field_name : record_type->fieldList().fieldNames()) {
            with_scope.add(field_name, variable_node.view.data());
        }

        with_statement->setBody(resolveWithStatementHelper(
            with_scope, with_statement_node, variable_index + 1, context));

        return with_statement;
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Scope &scope,
        const nodes::WithStatement &with_statement_node,
        const StatementAnalysisContext &context
    ) {
        return resolveWithStatementHelper(scope, with_statement_node, 0, context);
    }

    std::unique_ptr<sem::Statement>
    resolveStatement(
        sem::Scope &scope,
        const nodes::Statement &statement_node,
        const StatementAnalysisContext &context
    ) {
        LinkedListNode<label_set_t> new_allowed_targets(context.allowed_goto_targets);

        if (statement_node.label) {
            pascal_integer_t label_value = statement_node.label->value;
            auto &block = scope.closestContainingBlock();
            auto it = block.labels_.find(label_value);

            if (it != block.labels_.end()) {
                const char *new_prefixing_occurrence
                    = statement_node.label->view.data();

                if (it->second.prefixing_occurrence) {
                    reporter_.err(new_prefixing_occurrence,
                        ec::AMBIGUOUS_LABEL,
                        "multiple statements prefixed by label {}", label_value);
                    reporter_.note(it->second.prefixing_occurrence,
                        "first statement prefixed by label {}", label_value);
                }
                else {
                    it->second.prefixing_occurrence = new_prefixing_occurrence;
                    new_allowed_targets.value.insert(label_value);
                }
            }
            else {
                reporter_.err(statement_node.label->view.data(),
                    ec::UNDEFINED_LABEL, "undefined label");
            }
        }

        auto statement = visit(*statement_node.unlabeled,
            [&](auto &node) -> std::unique_ptr<sem::Statement> {
                const auto &new_context = new_allowed_targets.value.empty()
                    ? context : context.withNewAllowedGotoTargets(&new_allowed_targets);
                return resolveUnlabeledStatement(scope, node, new_context);
            });

        if (!new_allowed_targets.value.empty())
            statement = std::make_unique<sem::StatementLabeled>(
                *new_allowed_targets.value.begin(), std::move(statement));

        return statement;
    }

    std::unique_ptr<sem::Statement>
    resolveUnsupportedBuiltinProcedure(
        sem::Scope &scope,
        std::span<const nodes::ActualParameter> actual_parameter_nodes,
        const char *actual_parameter_end_location,
        const StatementAnalysisContext &context
    ) {
        reporter_.err(actual_parameter_end_location,
            ec::UNSUPPORTED_FEATURE, "unsupported builtin procedure");
        return fallbackStatement();
    }

    std::unique_ptr<sem::Statement>
    resolveBuiltinWriteTyped(
        sem::Scope &scope,
        std::span<const nodes::ActualParameter> actual_parameter_nodes,
        const char *actual_parameter_end_location
    ) {
        // We already know that there is at least one parameter due to the check
        // made in `resolveBuiltinWrite`.

        checkNoFormattingSpecification(actual_parameter_nodes[0]);

        auto file = resolveExpressionAsVariableAccess(
            scope, actual_parameter_nodes[0].value);
        if (!file)
            return fallbackStatement();

        // We already know that the cast will succeed due to the check made in
        // `resolveBuiltinWrite`. The same expression cannot have different types
        // when resolved as an expression or as a variable reference.
        auto &file_variable_type
            = dynamic_cast<const sem::TypeFile &>(file->type(scope));

        if (actual_parameter_nodes.size() < 2) {
            reporter_.err(actual_parameter_end_location,
                ec::PARAMETER_COUNT_MISMATCH, "no value to be written");
            return fallbackStatement();
        }

        std::vector<std::unique_ptr<sem::Expression>> values;

        for (auto &parameter_node : std::views::drop(actual_parameter_nodes, 1)) {
            checkNoFormattingSpecification(parameter_node);

            auto parameter = resolveExpression(scope, parameter_node.value);
            auto &parameter_type = parameter->type(scope);
            auto &parameter_type_promoted = parameter_type.promoted();

            if (!parameter_type_promoted.isAssignmentCompatibleWith(
                *file_variable_type.componentType()
            )) {
                reporter_.err(parameter_node.value.view.data(),
                    ec::TYPE_MISMATCH,
                    "value of type \"{}\" is incompatible with file of type \"{}\"",
                    parameter_type_promoted.str(),
                    file_variable_type.componentType()->str());
                continue;
            }

            values.push_back(std::move(parameter));
        }

        if (values.empty())
            return fallbackStatement();

        return std::make_unique<sem::StatementProcedureWriteTyped>(
            std::move(file), std::move(values));
    }

    std::unique_ptr<sem::Statement>
    resolveBuiltinWrite(
        sem::Scope &scope,
        std::span<const nodes::ActualParameter> actual_parameter_nodes,
        const char *actual_parameter_end_location,
        const StatementAnalysisContext &
    ) {
        if (actual_parameter_nodes.empty()) {
            reporter_.err(actual_parameter_end_location,
                ec::PARAMETER_COUNT_MISMATCH, "no value to be written");
            return fallbackStatement();
        }

        reporter_.hold();

        auto parameter0 = resolveExpression(scope, actual_parameter_nodes[0].value);
        auto &parameter0_type = parameter0->type(scope);

        if (dynamic_cast<const sem::TypeFile *>(&parameter0_type)) {
            reporter_.unholdDiscard();
            return resolveBuiltinWriteTyped(
                scope, actual_parameter_nodes, actual_parameter_end_location);
        }

        std::unique_ptr<sem::VariableAccess> file;
        std::vector<std::unique_ptr<sem::Expression>> values;

        if (dynamic_cast<const sem::TypeText *>(&parameter0_type)) {
            reporter_.unholdDiscard();

            checkNoFormattingSpecification(actual_parameter_nodes[0]);
            file = resolveExpressionAsVariableAccess(scope, actual_parameter_nodes[0].value);

            if (!file)
                return fallbackStatement();

            if (actual_parameter_nodes.size() < 2) {
                reporter_.err(actual_parameter_end_location,
                    ec::PARAMETER_COUNT_MISMATCH, "no value to be written");
                return fallbackStatement();
            }
        }
        else {
            reporter_.unhold();

            reporter_.err(actual_parameter_end_location,
                ec::UNSUPPORTED_FEATURE, "\"write\" with an implicit file is not supported");
            return fallbackStatement();
        }

        for (auto &parameter_node : std::views::drop(actual_parameter_nodes, 1)) {
            // TODO: deal with formatting specifications

            auto parameter = resolveExpression(scope, parameter_node.value);
            auto &parameter_type = parameter->type(scope);
            auto &parameter_type_promoted = parameter_type.promoted();

            if (
                &parameter_type_promoted == &sem::TypeInteger::instance()
                    || &parameter_type_promoted == &sem::TypeReal::instance()
                    || &parameter_type_promoted == &sem::TypeChar::instance()
                    || &parameter_type_promoted == &sem::TypeBoolean::instance()
            ) {
                values.push_back(std::move(parameter));
            }
            else if (
                auto *array_type
                    = dynamic_cast<const sem::TypeArray *>(&parameter_type_promoted);
                array_type && array_type->isString()
            ) {
                values.push_back(std::move(parameter));
            }
            else {
                reporter_.err(parameter_node.value.view.data(),
                    ec::TYPE_MISMATCH,
                    "value type \"{}\" is not integer, real, char, boolean or a string type",
                    parameter_type_promoted.str());
            }
        }

        if (values.empty())
            return fallbackStatement();

        return std::make_unique<sem::StatementProcedureWriteText>(
            std::move(file), std::move(values));
    }

    void
    buildBlock(
        const nodes::Block &block_node,
        sem::Block &block,
        const label_set_t &allowed_outer_targets
    ) {
        analyzeLabelDeclarations(block_node, block);

        LinkedListNode<label_set_t> allowed_goto_targets(
            nullptr, allowed_outer_targets);

        // remove labels shadowed by labels from this block
        for (const auto kv : block.labels_)
            allowed_goto_targets.value.erase(kv.first);

        // add top-level labels from this block
        for (auto &statement_node : block_node.statement.statements)
            if (statement_node.label)
                allowed_goto_targets.value.insert(statement_node.label->value);

        collectDefiningOccurrencesInBlock(block.scope_, block_node);

        analyzeConstantDefinitions(block_node, block);
        analyzeTypeDefinitions(block_node, block);
        analyzeVariableDeclarations(block_node, block);
        analyzeSubroutineDeclarations(block_node, block, allowed_goto_targets.value);

        block.statement_ = resolveUnlabeledStatement(
            block.scope_, block_node.statement,
            StatementAnalysisContext{&allowed_goto_targets, nullptr});

        std::vector<const char *> nonprefixing_label_locations;

        for (const auto &label : block.labels_) {
            if (!label.second.prefixing_occurrence)
                nonprefixing_label_locations.push_back(label.second.defining_occurrence);
        }

        std::ranges::sort(nonprefixing_label_locations);

        for (const auto location : nonprefixing_label_locations)
            reporter_.err(location, ec::UNUSED_LABEL,
                "label that does not prefix a statement");
    }

    void
    build(const nodes::Program &program_node, sem::Program &program) {
        for (auto &parameter_node : program_node.parameter_declarations) {
            auto parameter_location = parameter_node.view.data();
            auto &parameter_name = parameter_node.spelling;

            auto [it, success] = program.parameters_.try_emplace(
                parameter_name, parameter_location);

            if (!success) {
                reporter_.err(parameter_location, ec::DUPLICATE_PROGRAM_PARAMETER,
                    "program parameter \"{}\" already defined", parameter_name);
                reporter_.note(it->second, "defining point of \"{}\"", parameter_name);
                continue;
            }

            if (parameter_name == "input"sv || parameter_name == "output"sv) {
                program.block_.scope_.add(parameter_node);
                program.block_.variables_.try_emplace(
                    parameter_name,
                    staticPtr(sem::TypeText::instance()));
            }
        }

        buildBlock(program_node.block, program.block_, label_set_t{});

        for (const auto &[parameter_name, parameter_location] : program.parameters_) {
            auto it = program.block_.variables_.find(parameter_name);
            if (it == program.block_.variables_.end())
                reporter_.err(parameter_location, ec::MISSING_PROGRAM_PARAMETER_VARIABLE,
                    "program parameter \"{}\" has no corresponding variable",
                    parameter_name);
        }
    }

private:
    Reporter &reporter_;
};

const std::unordered_map<std::string_view, ProgramBuilder::builtin_procedure_call_f>
ProgramBuilder::BUILTIN_PROCEDURES = {
    {"dispose"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"get"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"new"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"pack"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"page"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"put"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"read"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"readln"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"reset"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"rewrite"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"unpack"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
    {"write"sv, &ProgramBuilder::resolveBuiltinWrite},
    {"writeln"sv, &ProgramBuilder::resolveUnsupportedBuiltinProcedure},
};

std::unique_ptr<sem::Program>
analyze(const nodes::Program &program_node, Reporter &reporter) {
    auto program = std::make_unique<sem::Program>();
    ProgramBuilder(reporter).build(program_node, *program);
    return program;
}
