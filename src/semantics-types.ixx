// SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
//
// SPDX-License-Identifier: MPL-2.0

module;

#include <cassert>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

export module semantics:types;

export import :core;

import parsing;
import utilities;

using namespace std::literals;

namespace sem {

template <typename T, typename Base = Type>
class TypeBuiltin : public Base {
public:
    using Base::Base;

    static const T &
    instance() {
        static constexpr T t;
        return t;
    }
};

template <typename T, typename Base = Type>
class TypeBuiltinAccessible : public TypeBuiltin<T, Base> {
public:
    using TypeBuiltin<T, Base>::TypeBuiltin;

    std::string
    str() const override { return T::NAME.str(); }
};

export
class TypeBoolean final : public TypeBuiltinAccessible<TypeBoolean, TypeOrdinal> {
public:
    static inline constexpr Cisref NAME = "Boolean"_ci;

    pascal_integer_t
    smallestOrdinal() const override { return 0; }

    pascal_integer_t
    largestOrdinal() const override { return 1; }

private:
    TypeBoolean() = default;
    friend class TypeBuiltin;
};

export
class TypeChar final : public TypeBuiltinAccessible<TypeChar, TypeOrdinal> {
public:
    static inline constexpr Cisref NAME = "char"_ci;

    pascal_integer_t
    smallestOrdinal() const override { return 0; }

    pascal_integer_t
    largestOrdinal() const override {
        return std::numeric_limits<unsigned char>::max();
    }

private:
    TypeChar() = default;
    friend class TypeBuiltin;
};

export
class TypeInteger final : public TypeBuiltinAccessible<TypeInteger, TypeOrdinal> {
public:
    static inline constexpr Cisref NAME = "integer"_ci;

    pascal_integer_t
    smallestOrdinal() const override { return -PASCAL_INTEGER_MAX; }

    pascal_integer_t
    largestOrdinal() const override { return PASCAL_INTEGER_MAX; }

    bool
    isAssignmentCompatibleWith(const Type &other) const override;

private:
    TypeInteger() = default;
    friend class TypeBuiltin;
};

export
class TypeReal final : public TypeBuiltinAccessible<TypeReal> {
public:
    static inline constexpr Cisref NAME = "real"_ci;

private:
    TypeReal() = default;
    friend class TypeBuiltin;
};

export
class ConstantEnumerated;

export
class TypeEnumerated final
    : public TypeOrdinal, public std::enable_shared_from_this<TypeEnumerated>
{
public:
    ~TypeEnumerated();

    static std::shared_ptr<const TypeEnumerated>
    make(std::span<const Cisref> constant_names);

    std::vector<std::shared_ptr<const ConstantEnumerated>>
    constants() const;

    std::string
    str() const override;

    pascal_integer_t
    smallestOrdinal() const override { return 0; }

    pascal_integer_t
    largestOrdinal() const override;

private:
    explicit
    TypeEnumerated(std::span<const Cisref> constant_names);

    std::vector<ConstantEnumerated> constants_;
};

export
class TypeSubrange final : public TypeOrdinal {
public:
    TypeSubrange(
        ConstantOrdinal::ptr_t smallest_value,
        ConstantOrdinal::ptr_t largest_value
    ) : smallest_value_(smallest_value), largest_value_(largest_value) {}

    std::string
    str() const override {
        return smallest_value_->str() + ".."s + largest_value_->str();
    }

    const TypeOrdinal &
    fullRange() const override {
        // It shouldn't be possible to form subranges of subranges,
        // so `*smallest_value_->typeOrdinal()` should be sufficient, but
        // just in case, we also call `fullRange` on that.
        return smallest_value_->typeOrdinal()->fullRange();
    }

    pascal_integer_t
    smallestOrdinal() const override {
        return smallest_value_->ordinalNumber();
    }

    pascal_integer_t
    largestOrdinal() const override {
        return largest_value_->ordinalNumber();
    }

    TypeOrdinal::ptr_t
    hostType() const { return smallest_value_->typeOrdinal(); }

private:
    ConstantOrdinal::ptr_t smallest_value_;
    ConstantOrdinal::ptr_t largest_value_;
};

export
class TypeSubrangeDynamic : public TypeOrdinalDynamic {
public:
    TypeSubrangeDynamic(
        const Cisref &smallest_bound_id,
        const Cisref &largest_bound_id,
        TypeOrdinal::ptr_t bound_type
    )
        : smallest_bound_id_(smallest_bound_id)
        , largest_bound_id_(largest_bound_id)
        , bound_type_(bound_type)
    {}

    std::string
    str() const override {
        return std::format("{}..{}: {}", smallest_bound_id_, largest_bound_id_, bound_type_->str());
    };

    const TypeOrdinal &
    fullRange() const override {
        // Unlike TypeSubrange, the bound type _can_ be a subrange, so we need
        // to delegate the `fullRange` call to it.
        return bound_type_->fullRange();
    }

    Cisref
    smallestBoundId() const { return smallest_bound_id_; }

    Cisref
    largestBoundId() const { return largest_bound_id_; }

    TypeOrdinal::ptr_t
    boundType() const { return bound_type_; }

private:
    Cisref smallest_bound_id_, largest_bound_id_;
    TypeOrdinal::ptr_t bound_type_;
};

export
class TypeArray final : public Type {
public:
    TypeArray(
        TypeOrdinalDynamic::ptr_t index_type,
        Type::ptr_t component_type,
        bool is_packed
    ) : index_type_(index_type), component_type_(component_type), is_packed_(is_packed) {}

    std::string
    str() const override {
        return (is_packed_ ? "packed "s : ""s)
            + "array ["s + index_type_->str() + "] of " + component_type_->str();
    }

    bool
    canBeFileComponent() const override {
        return component_type_->canBeFileComponent();
    }

    bool
    isString() const {
        auto index_type_subrange
            = std::dynamic_pointer_cast<const TypeSubrange>(index_type_);

        return is_packed_ && index_type_subrange
            && index_type_subrange->hostType().get() == &sem::TypeInteger::instance()
            && index_type_subrange->smallestOrdinal() == 1
            && index_type_subrange->largestOrdinal() > 1
            && component_type_.get() == &sem::TypeChar::instance();
    }

    pascal_integer_t
    stringLength() const {
        assert(isString());

        return std::dynamic_pointer_cast<const TypeSubrange>(
            index_type_)->largestOrdinal();
    }

    bool
    isCompatibleWith(const Type &other) const override {
        if (isString())
            if (auto *other_array = dynamic_cast<const TypeArray *>(&other))
                return other_array->isString()
                    && stringLength() == other_array->stringLength();
        return Type::isCompatibleWith(other);
    }

    bool
    isConformableWith(const Type &type) const override;

    bool
    isEquivalent(const Type &type) const override;

    TypeOrdinalDynamic::ptr_t
    indexType() const { return index_type_; }

    Type::ptr_t
    componentType() const { return component_type_; }

    bool
    isPacked() const { return is_packed_; }

private:
    TypeOrdinalDynamic::ptr_t index_type_;
    Type::ptr_t component_type_;
    bool is_packed_;
};

export
class TypeFileLike : public Type {
public:
    bool
    canBeFileComponent() const override {
        return false;
    }

    virtual Type::ptr_t
    componentType() const = 0;
};

export
class TypeFile final : public TypeFileLike {
public:
    TypeFile(
        Type::ptr_t component_type,
        bool is_packed
    ) : component_type_(component_type), is_packed_(is_packed) {
        assert(component_type_->canBeFileComponent());
    }

    std::string
    str() const override {
        return (is_packed_ ? "packed "s : ""s)
            + "file of "s + component_type_->str();
    }

    Type::ptr_t
    componentType() const override { return component_type_; }

private:
    Type::ptr_t component_type_;
    bool is_packed_;
};

export
class TypeText final : public TypeBuiltinAccessible<TypeText, TypeFileLike> {
public:
    static inline constexpr Cisref NAME = "text"_ci;

    Type::ptr_t
    componentType() const override {
        return staticPtr(TypeChar::instance());
    }

private:
    TypeText() = default;
    friend class TypeBuiltin;
};

export
class FieldList;
struct Variant;

export
class VariantPart {
public:
    explicit VariantPart(TypeOrdinal::ptr_t tag_type);

    VariantPart(const VariantPart &);
    VariantPart &
    operator =(const VariantPart &);

    ~VariantPart();

    TypeOrdinal::ptr_t
    tagType() const { return tag_type_; }

    const std::optional<Cisref> &
    tagField() const { return tag_field_; }

    void
    setTagField(const Cisref &tag_field) {
        tag_field_ = tag_field;
    }

    std::span<const Variant>
    variants() const;

    const FieldList &
    variantByOrdinal(pascal_integer_t ordinal) const;

    void
    addVariant(
        std::span<ConstantOrdinal::ptr_t> case_constants,
        const FieldList &fields
    );

private:
    TypeOrdinal::ptr_t tag_type_;
    std::optional<Cisref> tag_field_;
    std::vector<Variant> variants_;

    std::unordered_map<pascal_integer_t, std::size_t> variant_indexes_by_ordinal_;
};

export
class FieldList {
public:
    FieldList() = default;

    // Returns all fields' names (including the ones from the variant part)
    // in an arbitrary order.
    std::vector<Cisref>
    fieldNames() const;

    bool
    hasField(const Cisref &name) const {
        return field_descriptions_.contains(name);
    }

    Type::ptr_t
    fieldType(const Cisref &name) const {
        return field_descriptions_.at(name).type;
    }

    bool
    fieldIsTag(const Cisref &name) const {
        return field_descriptions_.at(name).is_tag;
    }

    void
    addField(const Cisref &name, Type::ptr_t type) {
        own_field_names_.push_back(name);
        field_descriptions_.emplace(name, type);
    }

    const std::optional<VariantPart> &
    variantPart() const { return variant_part_; }

    void
    setVariantPart(const VariantPart &variant_part);

    bool
    canBeFileComponent() const;

private:
    struct FieldDescription {
        Type::ptr_t type;
        bool is_tag = false;
    };

    std::vector<Cisref> own_field_names_;
    std::unordered_map<Cisref, FieldDescription> field_descriptions_;
    std::optional<VariantPart> variant_part_;
};

struct Variant {
    std::vector<ConstantOrdinal::ptr_t> case_constants;
    FieldList fields;
};

export
class TypeRecord final : public Type {
public:
    TypeRecord(
        const FieldList &field_list,
        bool is_packed
    ) : field_list_(field_list), is_packed_(is_packed) {}

    std::string
    str() const override {
        // A full string representation would be annoying to build and possibly
        // quite long. For simplicity, just show that it's a record.
        return "<record>"s;
    }

    bool
    canBeFileComponent() const override {
        return field_list_.canBeFileComponent();
    }

    const FieldList &
    fieldList() const { return field_list_; }

    bool
    isPacked() const { return is_packed_; }

private:
    FieldList field_list_;
    bool is_packed_;
};

export
class TypeSet final : public Type {
public:
    TypeSet(
        TypeOrdinal::ptr_t base_type,
        bool is_packed
    ) : base_type_(base_type), is_packed_(is_packed)
    {
        if (auto subrange_type
            = std::dynamic_pointer_cast<const TypeSubrange>(base_type_)
        )
            host_type_set_ = std::make_unique<TypeSet>(
                subrange_type->hostType(), is_packed);
    }

    std::string
    str() const override {
        return (is_packed_ ? "packed "s : ""s)
            + "set of "s + base_type_->str();
    }

    const TypeSet &
    promoted() const override {
        /*
            This isn't quite right according to the standard, but it should
            be right enough.

            The standard says that sets of subrange types should be treated as
            [un]packed-canonical-set-of-T, where T is the host type of the subrange.
            However, we don't implement canonical set types, and just create a new
            set type of the host type. When we typecheck set operators (which are
            defined to only accept canonical set types), we just make sure that
            the operand types have the same base type and packedness.

            This deviation from the standard is for a few reasons:

            1. To implement canonicity, we would need to maintain some sort
               of global store of canonical set types, which would add complexity.
            2. The wording in the standard implies that set operators cannot be
               used with sets of non-subrange types (since they are _not_ treated
               as canonical sets), which doesn't make much sense. By allowing
               operators to accept any sets, we fix this.

            This change should not make any programs which are valid under the
            standard rules to become invalid or change semantics.
        */

        return host_type_set_ ? *host_type_set_ : *this;
    }

    bool
    isCompatibleWith(const Type &other) const override {
        if (auto *other_set = dynamic_cast<const TypeSet *>(&other))
            return is_packed_ == other_set->is_packed_
                && base_type_->isCompatibleWith(*other_set->base_type_);
        return Type::isCompatibleWith(other);
    }

    TypeOrdinal::ptr_t
    baseType() const { return base_type_; }

private:
    TypeOrdinal::ptr_t base_type_;
    bool is_packed_;

    std::unique_ptr<TypeSet> host_type_set_;
};

// A synthetic type created to be the type of the `[...]` expression
// (with at least one member).
export
class TypeSetIncomplete final : public Type {
public:
    explicit
    TypeSetIncomplete(const TypeOrdinal &base_type) : base_type_(base_type)
    {
        // The base type should never be a subrange, since it's supposed to be
        // the promoted type of a member expression.
        assert(!dynamic_cast<const TypeSubrange *>(&base_type_));
    }

    std::string
    str() const override {
        return "[packed] set of "s + base_type_.str();
    }

    const TypeSetIncomplete &
    promoted() const override {
        return *this;
    }

    bool
    isCompatibleWith(const Type &other) const override {
        if (auto *other_set = dynamic_cast<const TypeSetIncomplete *>(&other))
            return base_type_.isCompatibleWith(other_set->base_type_);
        return Type::isCompatibleWith(other);
    }

    bool
    isAssignmentCompatibleWith(const Type &other) const override {
        if (auto *other_set = dynamic_cast<const TypeSet *>(&other))
            return base_type_.isCompatibleWith(*other_set->baseType());
        return Type::isAssignmentCompatibleWith(other);
    }

    const TypeOrdinal &
    baseType() const { return base_type_; }

private:
    // HACK: this should really hold a TypeOrdinal::ptr_t, but we can't get one
    // when this type is created, because the host type is derived from the type
    // of the member expression(s), and that's only available as a raw reference.
    // This shouldn't be a problem, since the `ExpressionSetConstructor` owns both
    // the `TypeSetIncomplete` and the member expression (and therefore its type),
    // so the reference should never become dead.
    // For safety, instances of this should never be created outside
    // `ExpressionSetConstructor`.
    const TypeOrdinal &base_type_;
};

// A synthetic type created to be the type of the `[]` expression.
export
class TypeSetAny final : public TypeBuiltin<TypeSetAny> {
public:
    std::string
    str() const override { return "[packed] set of <??\?>"s; }

    bool
    isAssignmentCompatibleWith(const Type &other) const override {
        if (dynamic_cast<const TypeSet *>(&other)) return true;
        if (dynamic_cast<const TypeSetIncomplete *>(&other)) return true;
        return Type::isAssignmentCompatibleWith(other);
    }

private:
    TypeSetAny() = default;
    friend class TypeBuiltin;
};

export
class TypePointer final : public Type {
public:
    TypePointer(
        const Block &domain_type_block, const Cisref &domain_type_name
    ) : domain_type_block_(domain_type_block), domain_type_name_(domain_type_name)
    {}

    std::string
    str() const override {
        // We must not resolve the domain type and call `str` on it, since we
        // might get into a recursive loop.
        return std::format("^{}", domain_type_name_);
    }

    Type::ptr_t
    domainType() const;

private:
    const Block &domain_type_block_;
    Cisref domain_type_name_;
};

// A synthetic type created to be the type of the `nil` expression.
export
class TypePointerAny final : public TypeBuiltin<TypePointerAny> {
public:
    std::string
    str() const override { return "^<??\?>"s; }

    bool
    isAssignmentCompatibleWith(const Type &other) const override {
        if (dynamic_cast<const TypePointer *>(&other)) return true;
        return Type::isAssignmentCompatibleWith(other);
    }

private:
    TypePointerAny() = default;
    friend class TypeBuiltin;
};

}
