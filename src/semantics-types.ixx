module;

#include <cassert>
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
    instance();

    std::string
    str() const override { return std::string(T::NAME); }
};

export
class TypeBoolean final : public TypeBuiltin<TypeBoolean, TypeOrdinal> {
public:
    static inline constexpr std::string_view NAME = "boolean"sv;

    pascal_integer_t
    smallestOrdinal() const override { return 0; }

    pascal_integer_t
    largestOrdinal() const override { return 1; }

private:
    TypeBoolean() = default;
    friend class TypeBuiltin;
};

export
class TypeChar final : public TypeBuiltin<TypeChar, TypeOrdinal> {
public:
    static inline constexpr std::string_view NAME = "char"sv;

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
class TypeInteger final : public TypeBuiltin<TypeInteger, TypeOrdinal> {
public:
    static inline constexpr std::string_view NAME = "integer"sv;

    pascal_integer_t
    smallestOrdinal() const override { return PASCAL_INTEGER_MIN; }

    pascal_integer_t
    largestOrdinal() const override { return PASCAL_INTEGER_MAX; }

    bool
    isAssignmentCompatibleWith(const DynamicType &other) const override;

private:
    TypeInteger() = default;
    friend class TypeBuiltin;
};

export
class TypeReal final : public TypeBuiltin<TypeReal> {
public:
    static inline constexpr std::string_view NAME = "real"sv;

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
    static std::shared_ptr<const TypeEnumerated>
    make(std::span<const std::string> constant_names);

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
    TypeEnumerated(std::span<const std::string> constant_names);

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
class TypeArray final : public Type {
public:
    TypeArray(
        TypeOrdinal::ptr_t index_type,
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

    bool
    isCompatibleWith(const DynamicType &other) const override {
        if (isString())
            if (auto *other_array = dynamic_cast<const TypeArray *>(&other))
                return other_array->isString()
                    && index_type_->largestOrdinal()
                        == other_array->index_type_->largestOrdinal();
        return Type::isCompatibleWith(other);
    }

    bool
    isConformableWith(const DynamicType &type_or_schema) const override;

    TypeOrdinal::ptr_t
    indexType() const { return index_type_; }

    Type::ptr_t
    componentType() const { return component_type_; }

    bool
    isPacked() const { return is_packed_; }

private:
    TypeOrdinal::ptr_t index_type_;
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
class TypeText final : public TypeBuiltin<TypeText, TypeFileLike> {
public:
    static inline constexpr std::string_view NAME = "text"sv;

    Type::ptr_t
    componentType() const override {
        return staticPtr(TypeChar::instance());
    }

private:
    TypeText() = default;
    friend class TypeBuiltin;
};

class FieldList;
struct Variant;

export
class VariantPart {
public:
    explicit VariantPart(TypeOrdinal::ptr_t tag_type)
        : tag_type_(tag_type)
    {}

    TypeOrdinal::ptr_t
    tagType() const { return tag_type_; }

    const std::optional<std::string> &
    tagField() const { return tag_field_; }

    void
    setTagField(const std::string &tag_field) {
        tag_field_ = tag_field;
    }

    std::span<const Variant>
    variants() const { return variants_; }

    void
    addVariant(
        std::span<ConstantOrdinal::ptr_t> case_constants,
        const FieldList &fields
    );

private:
    TypeOrdinal::ptr_t tag_type_;
    std::optional<std::string> tag_field_;
    std::vector<Variant> variants_;
};

export
class FieldList {
public:
    FieldList() = default;

    // Returns all fields' names (including the ones from the variant part)
    // in an arbitrary order.
    std::vector<std::string>
    fieldNames() const;

    bool
    hasField(const std::string &name) const {
        return field_descriptions_.contains(name);
    }

    Type::ptr_t
    fieldType(const std::string &name) const {
        return field_descriptions_.at(name).type;
    }

    bool
    fieldIsTag(const std::string &name) const {
        return field_descriptions_.at(name).is_tag;
    }

    void
    addField(const std::string &name, Type::ptr_t type) {
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

    std::vector<std::string> own_field_names_;
    std::unordered_map<std::string, FieldDescription> field_descriptions_;
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
    isCompatibleWith(const DynamicType &other) const override {
        if (auto *other_set = dynamic_cast<const TypeSet *>(&other))
            return is_packed_ == other_set->is_packed_
                && base_type_->isCompatibleWith(*other_set->base_type_);
        return Type::isCompatibleWith(other);
    }

private:
    TypeOrdinal::ptr_t base_type_;
    bool is_packed_;

    std::unique_ptr<TypeSet> host_type_set_;
};

export
class TypePointer final : public Type {
public:
    TypePointer(
        const Block &domain_type_block, const std::string &domain_type_name
    ) : domain_type_block_(domain_type_block), domain_type_name_(domain_type_name)
    {}

    std::string
    str() const override {
        // We must not resolve the domain type and call `str` on it, since we
        // might get into a recursive loop.
        return '^' + domain_type_name_;
    }

    Type::ptr_t
    domainType() const;

private:
    const Block &domain_type_block_;
    std::string domain_type_name_;
};

// A synthetic type created to be the type of the `nil` expression.
export
class TypePointerAny final : public TypeBuiltin<TypePointerAny> {
public:
    static inline constexpr std::string_view NAME = "^<???>"sv;

    bool
    isAssignmentCompatibleWith(const DynamicType &other) const override {
        if (dynamic_cast<const TypePointer *>(&other)) return true;
        return Type::isAssignmentCompatibleWith(other);
    }

private:
    TypePointerAny() = default;
    friend class TypeBuiltin;
};

export
class ConformantArraySchema final : public DynamicType {
public:
    ConformantArraySchema(
        const std::string &smallest_bound, const std::string &largest_bound,
        TypeOrdinal::ptr_t bound_type, DynamicType::ptr_t component_type,
        bool is_packed
    )
        : smallest_bound_(smallest_bound)
        , largest_bound_(largest_bound)
        , bound_type_(bound_type)
        , component_type_(component_type)
        , is_packed_(is_packed)
    {}

    std::string
    str() const override {
        // the following gives ICE in MSVC++ 17.2.5:
        //return std::format("{}array [{}..{}: {}] of {}",
        //    is_packed_ ? "packed "sv : ""sv,
        //    smallest_bound_, largest_bound_, bound_type_->str(),
        //    component_type_->str());
        return (is_packed_ ? "packed "s : ""s) +
            "array ["s + smallest_bound_ + ".."s + largest_bound_ + ": "s +
            bound_type_->str() + "] of "s + component_type_->str();
    }

    // Schemas can't really be file components, but we need to implement this
    // method anyway so that analysis can determine whether a schema should be
    // allowed as a value parameter.
    bool
    canBeFileComponent() const override {
        return component_type_->canBeFileComponent();
    }

    bool
    isConformableWith(const DynamicType &type_or_schema) const override;

    const std::string &
    smallestBound() const { return smallest_bound_; }

    const std::string &
    largestBound() const { return largest_bound_; }

    TypeOrdinal::ptr_t
    boundType() const { return bound_type_; }

    DynamicType::ptr_t
    componentType() const { return component_type_; }

    bool
    isPacked() const { return is_packed_; }

private:
    std::string smallest_bound_;
    std::string largest_bound_;
    TypeOrdinal::ptr_t bound_type_;
    DynamicType::ptr_t component_type_;
    bool is_packed_;
};

}
