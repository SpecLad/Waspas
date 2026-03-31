module;

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <string>
#include <variant>
#include <vector>

module semantics;

import utilities;

using namespace std::literals;

const sem::TypeOrdinalDynamic &
sem::TypeOrdinalDynamic::promoted() const { return fullRange(); }

// This should really be defined inline, but doing that
// makes VC++ generate multiple definitions for the t symbol.
// TODO: report compiler bug
template <typename T, typename Base>
const T &
sem::TypeBuiltin<T, Base>::instance() {
    static constexpr T t;
    return t;
}

bool
sem::TypeInteger::isAssignmentCompatibleWith(const Type &other) const {
    if (&other == &TypeReal::instance()) return true;
    return Type::isAssignmentCompatibleWith(other);
}

const sem::FieldList &
sem::VariantPart::variantByOrdinal(pascal_integer_t ordinal) const {
    std::size_t variant_index = variant_indexes_by_ordinal_.at(ordinal);
    return variants_.at(variant_index).fields;
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

    for (const auto &case_constant : case_constants)
        variant_indexes_by_ordinal_[case_constant->ordinalNumber()]
            = variants_.size() - 1;
}

sem::TypeEnumerated::~TypeEnumerated() = default;

std::shared_ptr<const sem::TypeEnumerated>
sem::TypeEnumerated::make(std::span<const std::string> constant_names) {
    return std::shared_ptr<sem::TypeEnumerated>(new TypeEnumerated(constant_names));
}

sem::TypeEnumerated::TypeEnumerated(
    std::span<const std::string> constant_names
) {
    assert(!constant_names.empty());
    assert(constant_names.size() - 1 <= std::size_t(PASCAL_INTEGER_MAX));

    constants_.reserve(constant_names.size());

    for (auto i : std::views::iota(std::size_t(0), constant_names.size()))
        constants_.push_back(ConstantEnumerated(*this, i, constant_names[i]));
}

std::vector<std::shared_ptr<const sem::ConstantEnumerated>>
sem::TypeEnumerated::constants() const {
    auto self_ptr = shared_from_this();

    std::vector<std::shared_ptr<const sem::ConstantEnumerated>> result;
    result.reserve(constants_.size());

    for (const auto &c : constants_)
        result.push_back(std::shared_ptr<const sem::ConstantEnumerated>(self_ptr, &c));

    return result;
}

std::string
sem::TypeEnumerated::str() const {
    std::string s = "("s + constants_[0].str();

    for (const auto &c : std::views::drop(constants_, 1))
        s += ", "s + c.str();

    s += ")"s;
    return s;
}

pascal_integer_t
sem::TypeEnumerated::largestOrdinal() const {
    return constants_.size() - 1;
}

bool
sem::TypeArray::isConformableWith(const Type &type) const {
    auto *schema = dynamic_cast<const TypeArray *>(&type);
    if (!schema) return Type::isConformableWith(type);

    auto *index_specification = dynamic_cast<const TypeSubrangeDynamic *>(
        schema->indexType().get());
    if (!index_specification) return Type::isConformableWith(type);

    auto schema_bound_type = index_specification->boundType();

    if (!index_type_->isCompatibleWith(*schema_bound_type)) return false;

    if (auto index_type_static
        = dynamic_cast<const sem::TypeOrdinal *>(index_type_.get())
    ) {
        if (index_type_static->smallestOrdinal() < schema_bound_type->smallestOrdinal())
            return false;

        if (index_type_static->largestOrdinal() > schema_bound_type->largestOrdinal())
            return false;
    }
    else {
        // We can't check the index type's smallest/largest values, because
        // they aren't known at compile time. This has to be a runtime check.
    }

    if (is_packed_ != schema->isPacked()) return false;

    if (!component_type_->isConformableWith(*schema->componentType()))
        return false;

    return true;
}

bool
sem::TypeArray::isEquivalent(const Type &type) const {
    if (auto *other_array = dynamic_cast<const TypeArray *>(&type)) {
        auto *index_specification
            = dynamic_cast<const TypeSubrangeDynamic *>(index_type_.get());

        auto *other_index_specification
            = dynamic_cast<const TypeSubrangeDynamic *>(other_array->index_type_.get());

        if (!index_specification || !other_index_specification)
            return Type::isEquivalent(type);

        return
            index_specification->boundType() == other_index_specification->boundType()
            && component_type_->isEquivalent(*other_array->component_type_)
            && is_packed_ == other_array->is_packed_;
    }

    return false;
}

std::vector<std::string>
sem::FieldList::fieldNames() const {
    auto keys = std::views::keys(field_descriptions_);
    return std::vector<std::string>(keys.begin(), keys.end());
}

void
sem::FieldList::setVariantPart(const VariantPart &variant_part) {
    assert(!variant_part_);
    variant_part_ = variant_part;

    if (auto &tag_field = variant_part_->tagField())
        field_descriptions_.try_emplace(*tag_field, variant_part_->tagType(), true);

    for (auto &variant : variant_part_->variants())
        field_descriptions_.insert(
            variant.fields.field_descriptions_.begin(),
            variant.fields.field_descriptions_.end());
}

bool
sem::FieldList::canBeFileComponent() const {
    for (const auto &field : field_descriptions_)
        if (!field.second.type->canBeFileComponent())
            return false;

    if (variant_part_)
        for (const auto &variant : variant_part_->variants())
            if (!variant.fields.canBeFileComponent())
                return false;

    return true;
}

sem::Type::ptr_t
sem::TypePointer::domainType() const {
    return domain_type_block_.type(domain_type_name_);
}

sem::Signature::Signature(
    std::span<FormalParameterSection> parameters,
    Type::ptr_t result_type
)
    : parameters_(parameters.begin(), parameters.end())
    , result_type_(result_type)
{
    for (const auto &parameter : parameters_) {
        std::visit(overloaded{
            [this](const RegularParameterSection &rps) {
                for (const auto &name : rps.names())
                    regular_parameter_types_.try_emplace(name, rps.type());

                auto type = rps.type();

                while (
                    auto maybe_schema
                        = std::dynamic_pointer_cast<const TypeArray>(type)
                ) {
                    if (auto index_specification
                        = std::dynamic_pointer_cast<const TypeSubrangeDynamic>(
                            maybe_schema->indexType())
                    ) {
                        bound_types_.try_emplace(
                            index_specification->smallestBoundId(),
                            index_specification->boundType());
                        bound_types_.try_emplace(
                            index_specification->largestBoundId(),
                            index_specification->boundType());
                        type = maybe_schema->componentType();
                    }
                    else {
                        // not actually a schema
                        break;
                    }
                }
            },
            [this](const SubroutineParameterSpecification &sps) {
                subroutine_parameter_signatures_.try_emplace(
                    sps.name(), &sps.signature());
            },
        }, parameter.v);
    }
}

bool
sem::Signature::isCongruousWith(const Signature &other) const {
    if (parameters_.size() != other.parameters_.size())
        return false;

    return std::ranges::all_of(
        std::views::iota(std::size_t(0), parameters_.size()), [&](std::size_t i) {
            return parameters_[i].v.index() == other.parameters_[i].v.index()
                && std::visit(overloaded{
                    [&](const RegularParameterSection &rps) {
                        auto &other_rps = std::get<RegularParameterSection>(
                            other.parameters_[i].v);

                        return rps.isVariable() == other_rps.isVariable()
                            && rps.names().size() == other_rps.names().size()
                            && rps.type()->isEquivalent(*other_rps.type());
                    },
                    [&](const SubroutineParameterSpecification &sps) {
                        auto &other_sps = std::get<SubroutineParameterSpecification>(
                            other.parameters_[i].v);

                        return sps.signature().isCongruousWith(other_sps.signature())
                            && sps.signature().resultType()
                                == other_sps.signature().resultType();
                    },
                }, parameters_[i].v);
        }
    );

    return true;
}

const sem::Type &
sem::ExpressionBound::valueType(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    auto *subroutine = block->containingSubroutine();
    assert(subroutine);
    return subroutine->signature().boundType(id())->promoted();
}

// this is only defined out-of-line because TypeBuiltin::instance is.
const sem::Type &
sem::ExpressionNil::valueType(const Scope &) const {
    return TypePointerAny::instance();
}

// ditto
const sem::Type &
sem::ExpressionSetConstructor::valueType(const Scope &) const {
    if (type_) return *type_;
    return TypeSetAny::instance();
}

const sem::Type &
sem::ExpressionOperatorCommonType::valueType(const Scope &scope) const {
    const Type &left_type = left().valueType(scope);
    const Type &right_type = right().valueType(scope);

    if (left_type.isAssignmentCompatibleWith(right_type))
        return right_type;
    else
        return left_type;
}

const sem::Type &
sem::VariableAccessActivationResult::variableType(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    return *block->subroutine(id()).signature().resultType();
}

const sem::Type &
sem::VariableAccessFieldDesignatorId::variableType(const Scope &scope) const {
    auto *with = scope.parent(scopeIndex()).statementWith();
    assert(with);
    return *with->variableType().fieldList().fieldType(id());
}

const sem::Type &
sem::VariableAccessParameterId::variableType(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    auto *subroutine = block->containingSubroutine();
    assert(subroutine);
    return *subroutine->signature().regularParameterType(id());
}

const sem::Type &
sem::VariableAccessVariableId::variableType(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    return *block->variableType(id());
}

const sem::Type &
sem::VariableAccessBuffer::variableType(const Scope &scope) const {
    const auto &file_type = dynamic_cast<const TypeFileLike &>(
        file_->variableType(scope));

    return *file_type.componentType();
}

const sem::Type &
sem::VariableAccessDereference::variableType(const Scope &scope) const {
    const auto &pointer_type = dynamic_cast<const TypePointer &>(
        pointer_->variableType(scope));

    return *pointer_type.domainType();
}

const sem::Type &
sem::VariableAccessField::variableType(const Scope &scope) const {
    const auto &record_type = dynamic_cast<const TypeRecord &>(
        record_->variableType(scope));

    return *record_type.fieldList().fieldType(field_name_);
}

const sem::Type &
sem::VariableAccessIndexed::variableType(const Scope &scope) const {
    const auto &array_type = dynamic_cast<const TypeArray &>(
        array_->variableType(scope));

    return *array_type.componentType();
}

sem::Block builtin_block(nullptr);

sem::Program::Program() : block_(&builtin_block, this) {}

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
        // functions: abs, sqr, sin, cos, exp, ln, sqrt, arctan, trunc, round, ord, chr,
        //   succ, pred, odd, eof, eoln
    }

    template <typename ...Ts>
    static void
    addBuiltinTypes() {
        (builtin_block.types_.emplace(Ts::NAME, staticPtr(Ts::instance())), ...);
    }
} builtin_block_init;
