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

bool
sem::TypeInteger::isAssignmentCompatibleWith(const DynamicType &other) const {
    if (&other == &TypeReal::instance()) return true;
    return Type::isAssignmentCompatibleWith(other);
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
sem::TypeArray::isConformableWith(const DynamicType &type_or_schema) const {
    if (auto *schema = dynamic_cast<const ConformantArraySchema *>(&type_or_schema)) {
        auto schema_bound_type = schema->boundType();

        if (!index_type_->isCompatibleWith(*schema_bound_type)) return false;

        if (index_type_->smallestOrdinal() < schema_bound_type->smallestOrdinal())
            return false;

        if (index_type_->largestOrdinal() > schema_bound_type->largestOrdinal())
            return false;

        if (is_packed_ != schema->isPacked()) return false;

        return component_type_->isConformableWith(*schema->componentType());
    }

    return Type::isConformableWith(type_or_schema);
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

bool
sem::ConformantArraySchema::isConformableWith(const DynamicType &type_or_schema) const {
    if (auto *other_schema = dynamic_cast<const ConformantArraySchema *>(&type_or_schema)) {
        if (!bound_type_->isCompatibleWith(*other_schema->bound_type_)) return false;

        // We can't check the index type's smallest/largest values, because
        // they aren't known at compile time. This has to be a runtime check.

        if (is_packed_ != other_schema->is_packed_) return false;

        return component_type_->isConformableWith(*other_schema->component_type_);
    }

    return false;
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
                    auto schema
                        = std::dynamic_pointer_cast<const ConformantArraySchema>(type)
                ) {
                    bound_types_.try_emplace(
                        schema->smallestBound(), schema->boundType());
                    bound_types_.try_emplace(
                        schema->largestBound(), schema->boundType());
                    type = schema->componentType();
                }
            },
            [this](const SubroutineParameterSpecification &sps) {
                subroutine_parameter_signatures_.try_emplace(
                    sps.name(), &sps.signature());
            },
        }, parameter.v);
    }
}

const sem::DynamicType &
sem::ExpressionBound::type(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    auto *subroutine = block->containingSubroutine();
    assert(subroutine);
    return *subroutine->signature().boundType(id());
}

// this is only defined out-of-line because TypeBuiltin::instance is.
const sem::DynamicType &
sem::ExpressionNil::type(const Scope &) const {
    return TypePointerAny::instance();
}

const sem::DynamicType &
sem::VariableAccessActivationResult::type(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    return *block->subroutine(id()).signature().resultType();
}

const sem::DynamicType &
sem::VariableAccessFieldDesignatorId::type(const Scope &scope) const {
    auto *with = scope.parent(scopeIndex()).statementWith();
    assert(with);
    return *with->variableType().fieldList().fieldType(id());
}

const sem::DynamicType &
sem::VariableAccessParameterId::type(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    auto *subroutine = block->containingSubroutine();
    assert(subroutine);
    return *subroutine->signature().regularParameterType(id());
}

const sem::DynamicType &
sem::VariableAccessVariableId::type(const Scope &scope) const {
    auto *block = scope.parent(scopeIndex()).block();
    assert(block);
    return *block->variableType(id());
}

const sem::DynamicType &
sem::VariableAccessBuffer::type(const Scope &scope) const {
    const auto &file_type = dynamic_cast<const TypeFileLike &>(
        file_->type(scope));

    return *file_type.componentType();
}

const sem::DynamicType &
sem::VariableAccessDereference::type(const Scope &scope) const {
    const auto &pointer_type = dynamic_cast<const TypePointer &>(
        pointer_->type(scope));

    return *pointer_type.domainType();
}

const sem::DynamicType &
sem::VariableAccessField::type(const Scope &scope) const {
    const auto &record_type = dynamic_cast<const TypeRecord &>(
        record_->type(scope));

    return *record_type.fieldList().fieldType(field_name_);
}

const sem::DynamicType &
sem::VariableAccessIndexed::type(const Scope &scope) const {
    const auto &array_type = dynamic_cast<const TypeArray &>(
        array_->type(scope));

    return *array_type.componentType();
}

const sem::DynamicType &
sem::VariableAccessIndexedDynamic::type(const Scope &scope) const {
    const auto &schema = dynamic_cast<const ConformantArraySchema &>(
        dynamic_array_->type(scope));

    return *schema.componentType();
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
