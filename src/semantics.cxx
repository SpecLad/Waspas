module;

#include <algorithm>
#include <cassert>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

module semantics;

using namespace std::literals;

// This should really be defined inline, but doing that
// makes VC++ generate multiple definitions for the t symbol.
// TODO: report compiler bug
template <typename T, typename Base>
const T &
sem::TypeBuiltin<T, Base>::instance() {
    static constexpr T t;
    return t;
}

void
sem::VariantPart::addVariant(
    std::span<ConstantOrdinal::ptr_t> case_constants,
    const FieldList &fields
) {
    variants_.push_back(Variant{
        std::vector<ConstantOrdinal::ptr_t>(
            case_constants.begin(), case_constants.end()),
        fields,
    });
}

sem::TypeEnumerated::TypeEnumerated(
    std::span<const std::string> constant_names
) {
    assert(!constant_names.empty());
    assert(constant_names.size() - 1 <= std::size_t(PASCAL_INTEGER_MAX));

    for (auto i : std::views::iota(std::size_t(0), constant_names.size()))
        constants_.push_back(std::shared_ptr<ConstantEnumerated>(
            new ConstantEnumerated(*this, i, constant_names[i])));
}

std::string
sem::TypeEnumerated::str() const {
    std::string s = "("s + constants_[0]->str();

    for (const auto &c : std::views::drop(constants_, 1))
        s += ", "s + c->str();

    s += ")"s;
    return s;
}

// In situations where a type fails to resolve this function can be used
// to recover without dropping the declaration being analysed entirely.
template <typename T>
void
applyFallback(std::shared_ptr<const T> &ptr)
    requires std::is_base_of_v<T, sem::TypeInteger>
{
    if (!ptr)
        ptr = BuiltinBlockInitializer::getBuiltinPtr(sem::TypeInteger::instance);
}

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

sem::Block builtin_block(nullptr);

sem::Program::Program() : block_(&builtin_block) {}

struct BuiltinBlockInitializer {
    BuiltinBlockInitializer() {
        builtin_block.constants_.emplace("maxint",
            getBuiltinPtr(sem::ConstantInteger::instanceMax));

        builtin_block.constants_.emplace("false",
            getBuiltinPtr(sem::ConstantBoolean::instanceFalse));

        builtin_block.constants_.emplace("true",
            getBuiltinPtr(sem::ConstantBoolean::instanceTrue));

        for (const auto &c : builtin_block.constants_)
            builtin_block.scope_.addBuiltin(c.first);

        addBuiltinTypes<
            sem::TypeBoolean, sem::TypeChar, sem::TypeInteger, sem::TypeReal,
            sem::TypeText
        >();

        for (const auto &t : builtin_block.types_)
            builtin_block.scope_.addBuiltin(t.first, sem::DefiningOccurrence::TYPE);

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
                label_node.value, label_location, nullptr);

            if (!success) {
                reporter_.err(label_location, "duplicate-label",
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
        sem::Scope &scope,
        const nodes::Identifier &applied_occurrence_node,
        std::unordered_map<std::string, T> sem::Block::*map_member,
        std::string_view identifier_kind_str
    ) {
        const auto &spelling = applied_occurrence_node.spelling;
        auto applied_occurrence_location = applied_occurrence_node.view.data();

        auto generic_result = scope.lookup(spelling);
        if (!generic_result) {
            reporter_.err(applied_occurrence_location, "undefined-identifier",
                "undefined {} identifier \"{}\"", identifier_kind_str, spelling);
            return nullptr;
        }

        auto &[defining_scope, defining_occurrence] = *generic_result;

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
            reporter_.err(applied_occurrence_location, "use-before-definition",
                "identifier \"{}\" used before it was defined", spelling);
        }
        else {
            reporter_.err(applied_occurrence_location, "wrong-identifier-kind",
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
    resolveConstant(sem::Scope &scope, nodes::Constant &constant_node) {
        auto constant_location = constant_node.view.data();
        sem::Constant::ptr_t constant;

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
                        auto *ref_constant = lookupConstant(scope, id_node);
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
                reporter_.err(index_type_node->view.data(), "non-ordinal-type",
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
                "disallowed-file-component",
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
                reporter_.err(tag_type_node.view.data(), "non-ordinal-type",
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

                    auto ordinal_constant
                        = std::dynamic_pointer_cast<const sem::ConstantOrdinal>(constant);
                    if (!ordinal_constant) {
                        reporter_.err(constant_node->view.data(), "non-ordinal-type",
                            "case constant has non-ordinal type \"{}\"", constant->type().str());
                        return field_list;
                    }

                    if (!ordinal_constant->type().isCompatibleWith(*tag_type_ordinal)) {
                        reporter_.err(constant_node->view.data(), "type-mismatch",
                            "case constant type (\"{}\") is incompatible with tag type (\"{}\")",
                            ordinal_constant->type().str(), tag_type_ordinal->str());
                        return field_list;
                    }

                    auto ordinal = ordinal_constant->ordinalNumber();
                    if (!(tag_smallest_ordinal <= ordinal && ordinal <= tag_largest_ordinal)) {
                        reporter_.err(constant_node->view.data(), "out-of-range",
                            "case constant is not within the range of values of the tag type");
                        return field_list;
                    }

                    if (auto it = used_ordinals.find(ordinal); it != used_ordinals.end()) {
                        reporter_.err(constant_node->view.data(), "duplicate-case",
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
                reporter_.err(variant_part_node->view.data(), "missing-case",
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
        sem::Scope record_scope(&scope, nullptr);
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
                "non-ordinal-type", "set base type is non-ordinal");
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
            reporter_.err(enumerated_type_node.view.data(), "too-many-elements",
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

        auto enumerated_type
            = std::make_shared<sem::TypeEnumerated>(constant_names);

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
            reporter_.err(id_node.view.data(), "circular-definition",
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
                "undefined-identifier",
                "undefined type identifier \"{}\"", domain_type_name);
            return nullptr;
        }

        auto &[defining_scope, defining_occurrence] = *lookup_result;

        if (defining_occurrence.kind != sem::DefiningOccurrence::TYPE) {
            reporter_.err(pointer_type_node.domain_type.view.data(),
                "wrong-identifier-kind",
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
            defining_scope->block(), domain_type_name);
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
                "non-ordinal-type",
                "subrange bound has non-ordinal type \"{}\"", smallest->type().str());
            return nullptr;
        }

        if (&largest->type() != &smallest->type()) {
            reporter_.err(subrange_type_node.largest->view.data(),
                "type-mismatch",
                "largest subrange value has different type (\"{}\") "
                    "from smallest value type (\"{}\")",
                largest->type().str(), smallest->type().str());
            return nullptr;
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

                block.variables_.insert_or_assign(var_name_node.spelling, type);
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
                        reporter_.err(index_type_node.bound_type.view.data(), "non-ordinal-type",
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
        sem::Scope parameter_list_scope(&scope, nullptr);
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
                            "disallowed-parameter-type",
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
                        "disallowed-result-type",
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

    void
    analyzeSubroutineDeclarations(
        const nodes::Block &block_node,
        sem::Block &block
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
                                "missing-forward-declaration",
                                "delayed declaration with no preceding forward declaration");
                        }
                        else {
                            reporter_.err(subr_name_node.view.data(),
                                "duplicate-subroutine-declaration",
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
                            "mismatched-subroutine-declaration",
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
                buildBlock(*subr_decl_node.block, subroutine->block_);
                // TODO: check that the function contains at least one
                // assignment to the function identifier.
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
            reporter_.err(error_location, "missing-delayed-declaration",
                "forward declaration with no following delayed declaration");
    }

    std::unique_ptr<sem::StatementAssignment>
    resolveUnlabeledStatement(
        sem::Block &block, const nodes::AssignmentStatement &assignment_statement_node
    ) {
        // TODO: resolve the sides of the assignment
        return std::make_unique<sem::StatementAssignment>();
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Block &block, const nodes::CaseStatement &case_statement_node
    ) {
        // TODO: resolve case index; verify that the type is ordinal

        std::vector<sem::CaseListElement> case_list_elements;

        std::unordered_map<pascal_integer_t, const char *> used_ordinals;

        for (auto &element_node : case_statement_node.cases) {
            std::vector<sem::ConstantOrdinal::ptr_t> case_constants;

            for (auto &constant_node : element_node.constants) {
                auto constant = resolveConstant(block.scope_, *constant_node);
                if (!constant) continue;

                auto ordinal_constant
                    = std::dynamic_pointer_cast<const sem::ConstantOrdinal>(constant);
                if (!ordinal_constant) {
                    reporter_.err(constant_node->view.data(), "non-ordinal-type",
                        "case constant has non-ordinal type \"{}\"", constant->type().str());
                    continue;
                }

                // TODO: check that the constant has the same type as the index

                auto ordinal = ordinal_constant->ordinalNumber();

                if (auto it = used_ordinals.find(ordinal); it != used_ordinals.end()) {
                    reporter_.err(constant_node->view.data(), "duplicate-case",
                        "case constant already used");
                    reporter_.note(it->second, "previous occurrence of the case constant");
                    continue;
                }
                used_ordinals.insert_or_assign(ordinal, constant_node->view.data());

                case_constants.push_back(ordinal_constant);
            }

            auto statement = resolveStatement(block, element_node.statement);

            if (!case_constants.empty())
                case_list_elements.emplace_back(case_constants, std::move(statement));
        }

        if (case_list_elements.empty())
            return std::make_unique<sem::StatementEmpty>();

        return std::make_unique<sem::StatementCase>(std::move(case_list_elements));
    }

    std::unique_ptr<sem::StatementCompound>
    resolveUnlabeledStatement(
        sem::Block &block, const nodes::CompoundStatement &compound_statement_node
    ) {
        std::vector<std::unique_ptr<sem::Statement>> statements;

        for (auto &statement_node : compound_statement_node.statements) {
            statements.push_back(resolveStatement(block, statement_node));
        }

        return std::make_unique<sem::StatementCompound>(std::move(statements));
    }

    std::unique_ptr<sem::StatementEmpty>
    resolveUnlabeledStatement(
        sem::Block &block, const nodes::EmptyStatement &empty_statement_node
    ) {
        return std::make_unique<sem::StatementEmpty>();
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Block &block, const nodes::ForStatement &for_statement_node
    ) {
        const std::string &control_variable
            = for_statement_node.control_variable.spelling;

        auto it = block.variables_.find(control_variable);

        if (it == block.variables_.end()) {
            reporter_.err(for_statement_node.control_variable.view.data(),
                "undefined-identifier",
                "undefined variable identifier");
            return std::make_unique<sem::StatementEmpty>();
        }

        auto control_variable_type
            = std::dynamic_pointer_cast<const sem::TypeOrdinal>(it->second);

        if (!control_variable_type) {
            reporter_.err(for_statement_node.control_variable.view.data(),
                "non-ordinal-type",
                "control variable has non-ordinal type \"{}\"", it->second->str());
            return std::make_unique<sem::StatementEmpty>();
        }

        // TODO: resolve the initial and final value expressions; check their types
        // TODO: check for threatening statements

        return std::make_unique<sem::StatementFor>(
            control_variable,
            for_statement_node.direction,
            resolveStatement(block, for_statement_node.body));
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Block &block, const nodes::GotoStatement &goto_statement_node
    ) {
        pascal_integer_t label = goto_statement_node.label.value;
        std::size_t parent_index = 0;

        sem::Block *lookup_block = &block;

        for (; ; ) {
            auto it = lookup_block->labels_.find(label);
            if (it != lookup_block->labels_.end()) {
                // TODO: check goto target requirements
                return std::make_unique<sem::StatementGoto>(label, parent_index);
            }

            auto *parent_scope = lookup_block->scope_.parent();
            if (!parent_scope) break;

            lookup_block = parent_scope->block();
            assert(lookup_block); // all parent scopes of a block should be blocks

            ++parent_index;
        }

        reporter_.err(goto_statement_node.label.view.data(),
            "undefined-label", "undefined label");
        return std::make_unique<sem::StatementEmpty>();
    }

    std::unique_ptr<sem::StatementIf>
    resolveUnlabeledStatement(
        sem::Block &block, const nodes::IfStatement &if_statement_node
    ) {
        // TODO: resolve the expression; make sure it is of type boolean
        return std::make_unique<sem::StatementIf>(
            resolveStatement(block, if_statement_node.true_branch),
            if_statement_node.false_branch
                ? resolveStatement(block, *if_statement_node.false_branch)
                : nullptr);
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Block &block, const nodes::ProcedureStatement &procedure_statement_node
    ) {
        reporter_.err(procedure_statement_node.view.data(), "unsupported-feature",
            "procedure statements are not supported");
        return std::make_unique<sem::StatementEmpty>();
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Block &block, const nodes::RepeatStatement &repeat_statement_node
    ) {
        std::vector<std::unique_ptr<sem::Statement>> statements;

        for (auto &statement_node : repeat_statement_node.statements) {
            statements.push_back(resolveStatement(block, statement_node));
        }

        // TODO: resolve expression; check type

        return std::make_unique<sem::StatementRepeat>(std::move(statements));
    }

    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Block &block, const nodes::WhileStatement &while_statement_node
    ) {
        // TODO: resolve expression; check type

        return std::make_unique<sem::StatementWhile>(
            resolveStatement(block, while_statement_node.body));
    }

    template <typename T>
    std::unique_ptr<sem::Statement>
    resolveUnlabeledStatement(
        sem::Block &block, const T &statement_node
    ) {
        reporter_.err(statement_node.view.data(), "unsupported-feature",
            "statement type not supported");
        return nullptr;
    }

    std::unique_ptr<sem::Statement>
    resolveStatement(
        sem::Block &block, const nodes::Statement &statement_node
    ) {
        auto statement = visit(*statement_node.unlabeled,
            [&](auto &node) -> std::unique_ptr<sem::Statement> {
                return resolveUnlabeledStatement(block, node);
            });

        if (statement_node.label) {
            pascal_integer_t label_value = statement_node.label->value;
            auto it = block.labels_.find(label_value);

            if (it != block.labels_.end()) {
                const char *new_prefixing_occurrence
                    = statement_node.label->view.data();

                if (it->second.prefixing_occurrence) {
                    reporter_.err(new_prefixing_occurrence,
                        "ambiguous-label",
                        "multiple statements prefixed by label {}", label_value);
                    reporter_.note(it->second.prefixing_occurrence,
                        "first statement prefixed by label {}", label_value);
                }
                else {
                    it->second.prefixing_occurrence = new_prefixing_occurrence;
                    statement = std::make_unique<sem::StatementLabeled>(
                        label_value, std::move(statement));
                }
            }
            else {
                reporter_.err(statement_node.label->view.data(),
                    "undefined-label", "undefined label");
            }
        }

        return statement;
    }

    void
    buildBlock(const nodes::Block &block_node, sem::Block &block) {
        analyzeLabelDeclarations(block_node, block);

        collectDefiningOccurrencesInBlock(block.scope_, block_node);

        analyzeConstantDefinitions(block_node, block);
        analyzeTypeDefinitions(block_node, block);
        analyzeVariableDeclarations(block_node, block);
        analyzeSubroutineDeclarations(block_node, block);

        block.statement_ = resolveUnlabeledStatement(block, block_node.statement);

        std::vector<const char *> nonprefixing_label_locations;

        for (const auto &label : block.labels_) {
            if (!label.second.prefixing_occurrence)
                nonprefixing_label_locations.push_back(label.second.defining_occurrence);
        }

        std::ranges::sort(nonprefixing_label_locations);

        for (const auto location : nonprefixing_label_locations)
            reporter_.err(location, "unused-label",
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
                reporter_.err(parameter_location, "duplicate-program-parameter",
                    "program parameter \"{}\" already defined", parameter_name);
                reporter_.note(it->second, "defining point of \"{}\"", parameter_name);
                continue;
            }

            if (parameter_name == "input"sv || parameter_name == "output"sv) {
                program.block_.scope_.add(parameter_node);
                program.block_.variables_.insert_or_assign(
                    parameter_name,
                    BuiltinBlockInitializer::getBuiltinPtr(sem::TypeText::instance));
            }
        }

        buildBlock(program_node.block, program.block_);

        for (const auto &[parameter_name, parameter_location] : program.parameters_) {
            auto it = program.block_.variables_.find(parameter_name);
            if (it == program.block_.variables_.end())
                reporter_.err(parameter_location, "missing-program-parameter-variable",
                    "program parameter \"{}\" has no corresponding variable",
                    parameter_name);
        }
    }

private:
    Reporter &reporter_;
};

std::unique_ptr<sem::Program>
analyze(const nodes::Program &program_node, Reporter &reporter) {
    auto program = std::make_unique<sem::Program>();
    ProgramBuilder(reporter).build(program_node, *program);
    return program;
}
