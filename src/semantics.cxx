module;

#include <cassert>
#include <memory>
#include <ranges>
#include <string>
#include <variant>

module semantics;

import utilities;

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

std::vector<std::string>
sem::FieldList::allFieldNames() const {
    std::vector<std::string> names = field_names_;

    if (variant_part_) {
        if (auto tag_field = variant_part_->tagField()) {
            names.push_back(*tag_field);
        }

        for (auto &variant : variant_part_->variants()) {
            auto variant_field_names = variant.fields.allFieldNames();
            names.insert(names.end(),
                variant_field_names.begin(), variant_field_names.end());
        }
    }

    return names;
}

bool
sem::FieldList::canBeFileComponent() const {
    for (const auto &field : field_types_)
        if (!field.second->canBeFileComponent())
            return false;

    if (variant_part_)
        for (const auto &variant : variant_part_->variants())
            if (!variant.fields.canBeFileComponent())
                return false;

    return true;
}

sem::Signature::Signature(
    std::span<FormalParameterSection> parameters,
    Type::ptr_t result_type
)
    : parameters_(parameters.begin(), parameters.end())
    , result_type_(result_type)
{
    for (const auto &parameter : parameters_) {
        if (auto *rps = std::get_if<RegularParameterSection>(&parameter.v))
            for (const auto &name : rps->names())
                regular_parameter_types_.try_emplace(name, rps->type());
    }
}

sem::DynamicType::ptr_t
sem::VariableAccessActivationResult::type(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    return block->subroutine(id()).signature().resultType();
}

sem::DynamicType::ptr_t
sem::VariableAccessParameterId::type(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    auto *subroutine = block->containingSubroutine();
    assert(subroutine);
    return subroutine->signature().regularParameterType(id());
}

sem::DynamicType::ptr_t
sem::VariableAccessVariableId::type(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    return block->variableType(id());
}

sem::Block builtin_block(nullptr);

sem::Program::Program() : block_(&builtin_block) {}

struct BuiltinBlockInitializer {
    BuiltinBlockInitializer() {
        builtin_block.constants_.emplace("maxint",
            staticPtr(sem::ConstantInteger::instanceMax()));

        builtin_block.constants_.emplace("false",
            staticPtr(sem::ConstantBoolean::instanceFalse()));

        builtin_block.constants_.emplace("true",
            staticPtr(sem::ConstantBoolean::instanceTrue()));

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
        (builtin_block.types_.emplace(Ts::NAME, staticPtr(Ts::instance())), ...);
    }
} builtin_block_init;
