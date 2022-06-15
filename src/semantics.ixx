module;

#include <cassert>
#include <format>
#include <memory>
#include <ranges>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

export module semantics;

import parsing;
import reporting;

using namespace std::literals;

class ProgramBuilder;
struct BuiltinBlockInitializer;

namespace sem {

export
class Type {
public:
    Type() = default;

    virtual constexpr
    ~Type() = default;

    Type(const Type &) = delete;
    Type &operator =(const Type &) = delete;

    virtual std::string
    str() const = 0;

    virtual bool
    canBeFileComponent() const { return true; }
};

export
class TypeOrdinal : public Type {
public:
    bool
    isCompatibleWith(const TypeOrdinal &other) const {
        return &fullRange() == &other.fullRange();
    }

    virtual const TypeOrdinal &
    fullRange() const { return *this;}

    virtual pascal_integer_t
    smallestOrdinal() const = 0;

    virtual pascal_integer_t
    largestOrdinal() const = 0;
};

template <typename T, typename Base = Type>
class TypeBuiltin : public Base {
public:
    using Base::Base;

    static const T &
    instance() {
        static constexpr T t;
        return t;
    }

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

class ConstantOrdinal;
class ConstantEnumerated;

export
class TypeEnumerated final : public TypeOrdinal {
public:
    explicit
    TypeEnumerated(std::span<const std::string> constant_names);

    std::span<const std::shared_ptr<const ConstantEnumerated>>
    constants() const { return constants_; }

    std::string
    str() const override;

    pascal_integer_t
    smallestOrdinal() const override { return 0; }

    pascal_integer_t
    largestOrdinal() const override { return constants_.size() - 1; }

private:
    std::vector<std::shared_ptr<const ConstantEnumerated>> constants_;
};

export
class TypeSubrange final : public TypeOrdinal {
public:
    TypeSubrange(
        std::shared_ptr<const ConstantOrdinal> smallest_value,
        std::shared_ptr<const ConstantOrdinal> largest_value
    ) : smallest_value_(smallest_value), largest_value_(largest_value) {}

    std::string
    str() const override;

    const TypeOrdinal &
    fullRange() const override;

    pascal_integer_t
    smallestOrdinal() const override;

    pascal_integer_t
    largestOrdinal() const override;

private:
    std::shared_ptr<const ConstantOrdinal> smallest_value_;
    std::shared_ptr<const ConstantOrdinal> largest_value_;
};

export
class TypeArray final : public Type {
public:
    TypeArray(
        std::shared_ptr<const TypeOrdinal> index_type,
        std::shared_ptr<const Type> component_type,
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

private:
    std::shared_ptr<const TypeOrdinal> index_type_;
    std::shared_ptr<const Type> component_type_;
    bool is_packed_;
};

export
class TypeFileLike : public Type {
    bool
    canBeFileComponent() const override {
        return false;
    }
};

export
class TypeFile final : public TypeFileLike {
public:
    TypeFile(
        std::shared_ptr<const Type> component_type,
        bool is_packed
    ) : component_type_(component_type), is_packed_(is_packed) {
        assert(component_type_->canBeFileComponent());
    }

    std::string
    str() const override {
        return (is_packed_ ? "packed "s : ""s)
            + "file of "s + component_type_->str();
    }

private:
    std::shared_ptr<const Type> component_type_;
    bool is_packed_;
};

export
class TypeText final : public TypeBuiltin<TypeText, TypeFileLike> {
public:
    static inline constexpr std::string_view NAME = "text"sv;

private:
    TypeText() = default;
    friend class TypeBuiltin;
};

class FieldList;
struct Variant;

export
class VariantPart {
public:
    explicit VariantPart(std::shared_ptr<const TypeOrdinal> tag_type)
        : tag_type_(tag_type)
    {}

    void
    setTagField(const std::string &tag_field) {
        tag_field_ = tag_field;
    }

    std::span<const Variant>
    variants() const { return variants_; }

    void
    addVariant(
        std::span<std::shared_ptr<const ConstantOrdinal>> case_constants,
        const FieldList &fields
    );

private:
    std::shared_ptr<const TypeOrdinal> tag_type_;
    std::optional<std::string> tag_field_;
    std::vector<Variant> variants_;
};

export
class FieldList {
public:
    FieldList() = default;

    auto
    fields() const {
        // returns all fields in definition order
        return std::views::transform(fields_,
            [this](const std::string &name) -> decltype(auto) {
                return *field_types_.find(name);
            }
        );
    }

    void
    addField(const std::string &name, std::shared_ptr<const Type> type) {
        fields_.push_back(name);
        field_types_.emplace(name, type);
    }

    const std::optional<VariantPart> &
    variantPart() const { return variant_part_; }

    void
    setVariantPart(const VariantPart &variant_part) {
        variant_part_ = variant_part;
    }

private:
    std::vector<std::string> fields_;
    std::unordered_map<std::string, std::shared_ptr<const Type>> field_types_;
    std::optional<VariantPart> variant_part_;
};

struct Variant {
    std::vector<std::shared_ptr<const ConstantOrdinal>> case_constants;
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
        return fieldListCanBeInFileComponent(field_list_);
    }

private:
    static bool
    fieldListCanBeInFileComponent(const FieldList &field_list) {
        for (const auto &field : field_list.fields())
            if (!field.second->canBeFileComponent())
                return false;

        if (const auto &variant_part = field_list.variantPart())
            for (const auto &variant : variant_part->variants())
                if (!fieldListCanBeInFileComponent(variant.fields))
                    return false;

        return true;
    }

    FieldList field_list_;
    bool is_packed_;
};

export
class TypeSet final : public Type {
public:
    TypeSet(
        std::shared_ptr<const TypeOrdinal> base_type,
        bool is_packed
    ) : base_type_(base_type), is_packed_(is_packed) {}

    std::string
    str() const override {
        return (is_packed_ ? "packed "s : ""s)
            + "set of "s + base_type_->str();
    }

private:
    std::shared_ptr<const TypeOrdinal> base_type_;
    bool is_packed_;
};

class Block;

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

private:
    const Block &domain_type_block_;
    std::string domain_type_name_;
};

export
class Constant {
public:
    virtual constexpr
    ~Constant() = default;

    virtual const Type &
    type() const = 0;
};

export
class ConstantOrdinal : public Constant {
public:
    const TypeOrdinal &
    type() const override = 0;

    virtual std::string
    str() const = 0;

    virtual pascal_integer_t
    ordinalNumber() const = 0;
};

export
template <typename T, typename Value, typename Base = Constant>
class ConstantImpl : public Base {
public:
    explicit constexpr
    ConstantImpl(const Value &value) : value_(value)
    {}

    Value
    value() const { return value_; }

protected:
    Value value_;
};

export
class ConstantBoolean final
    : public ConstantImpl<ConstantBoolean, bool, ConstantOrdinal>
{
private:
    explicit constexpr
    ConstantBoolean(bool value) : ConstantImpl(value) {}

public:
    const TypeBoolean &
    type() const override { return TypeBoolean::instance(); }

    std::string
    str() const override { return value_ ? "true"s : "false"s; }

    pascal_integer_t
    ordinalNumber() const override { return value_ ? 1 : 0; }

    static const ConstantBoolean &
    instanceFalse() {
        static constexpr ConstantBoolean c(false);
        return c;
    }

    static const ConstantBoolean &
    instanceTrue() {
        static constexpr ConstantBoolean c(true);
        return c;
    }
};

export
class ConstantInteger final
    : public ConstantImpl<ConstantInteger, pascal_integer_t, ConstantOrdinal>
{
public:
    using ConstantImpl::ConstantImpl;

    const TypeInteger &
    type() const override { return TypeInteger::instance(); }

    std::string
    str() const override { return std::to_string(value_); }

    pascal_integer_t
    ordinalNumber() const override { return value_; }

    static const ConstantInteger &
    instanceMax() {
        static constexpr ConstantInteger c(PASCAL_INTEGER_MAX);
        return c;
    }
};

export
class ConstantReal final
    : public ConstantImpl<ConstantReal, pascal_real_t>
{
    using ConstantImpl::ConstantImpl;

    const TypeReal &
    type() const override { return TypeReal::instance(); }
};

export
class ConstantChar final
    : public ConstantImpl<ConstantChar, char, ConstantOrdinal>
{
    using ConstantImpl::ConstantImpl;

    const TypeChar &
    type() const override { return TypeChar::instance(); }

    std::string
    str() const override {
        if (value_ == '\'')
            return "''''"s;
        else
            return "'"s + value_ + "'";
    }

    pascal_integer_t
    ordinalNumber() const override { return pascal_integer_t(value_); }
};

export
class ConstantString final
    : public ConstantImpl<ConstantString, std::string>
{
public:
    explicit
    ConstantString(const std::string &value)
        : ConstantImpl(value)
        , type_(
            std::make_shared<TypeSubrange>(
                std::make_shared<ConstantInteger>(1),
                std::make_shared<ConstantInteger>(pascal_integer_t(value.size()))
            ),
            std::shared_ptr<const Type>(std::shared_ptr<void>(), &TypeChar::instance()),
            true
        )
    {
        assert(value.size() <= std::size_t(PASCAL_INTEGER_MAX));
    }

    const TypeArray &
    type() const override {
        return type_;
    }

private:
    TypeArray type_;
};

export
class ConstantEnumerated final : public ConstantOrdinal
{
public:
    const TypeEnumerated &
    type() const override { return type_; }

    std::string
    str() const override { return name_; }

    pascal_integer_t
    ordinalNumber() const override { return ordinal_number_; }

private:
    explicit
    ConstantEnumerated(
        const TypeEnumerated &type,
        pascal_integer_t ordinal_number,
        const std::string &name
    )
        : type_(type), ordinal_number_(ordinal_number), name_(name)
    {}

    const TypeEnumerated &type_;
    pascal_integer_t ordinal_number_;
    std::string name_;

    friend class TypeEnumerated;
};

struct DefiningOccurrence {
    const char *location;
    enum Kind { NOT_TYPE, TYPE } kind;
};

class Scope {
public:
    struct LookupResult {
        Scope *scope;
        DefiningOccurrence defining_occurrence;
    };

    Scope(Scope *parent, Block *block)
        : parent_(parent), block_(block) {}

    void
    add(
        const nodes::Identifier &id_node,
        DefiningOccurrence::Kind kind = DefiningOccurrence::NOT_TYPE
    ) {
        // Ignore duplicate identifiers; this is so that when the analysis
        // logic reports the error, it can note the first defining occurrence.
        dos_.try_emplace(id_node.spelling,
            DefiningOccurrence{id_node.view.data(), kind});
    }

    void
    addBuiltin(
        const std::string &id,
        DefiningOccurrence::Kind kind = DefiningOccurrence::NOT_TYPE
    ) {
        auto [it, success]
            = dos_.try_emplace(id, DefiningOccurrence{nullptr, kind});
        assert(success); // builtins should not be duplicated
    }

    Block *
    block() { return block_; }

    Block &
    closestContainingBlock() {
        if (block_) return *block_;

        // at least one scope in the chain has to be associated with a block
        assert(parent_);

        return parent_->closestContainingBlock();
    }

    std::optional<LookupResult>
    lookup(const std::string &id) {
        auto it = dos_.find(id);
        if (it != dos_.end())
            return LookupResult{this, it->second};

        if (parent_)
            return parent_->lookup(id);

        return std::nullopt;
    }

    DefiningOccurrence
    lookupShallowUnsafe(const std::string &id) const {
        return dos_.at(id);
    }

private:
    Scope *parent_;
    Block *block_;

    std::unordered_map<std::string, DefiningOccurrence> dos_;
};

export
class Subroutine {
public:
    Subroutine(const char *declaration_location)
        : last_declaration_location_(declaration_location) {}

private:
    const char *last_declaration_location_;
    bool is_function_; // this will likely be replaced by the signature later

    friend class ProgramBuilder;
};

export
class Block {
public:
    Block(Block *parent_block)
        : scope_(parent_block ? &parent_block->scope_ : nullptr, this)
    {}

private:
    Scope scope_;

    std::unordered_map<pascal_integer_t, const char *> labels_;
    std::unordered_map<std::string, std::shared_ptr<const Constant>> constants_;
    std::unordered_map<std::string, std::shared_ptr<const Type>> types_;
    std::unordered_map<std::string, std::shared_ptr<const Type>> variables_;
    std::unordered_map<std::string, Subroutine> subroutines_;

    friend class ProgramBuilder;
    friend struct BuiltinBlockInitializer;
};

export
class Program {
    Program();

    std::unordered_map<std::string, const char *> parameters_;

    Block block_;

    friend class ProgramBuilder;
};

}

export
sem::Program
analyze(const nodes::Program &program_node, Reporter &reporter);
