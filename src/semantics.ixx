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
#include <variant>

export module semantics;

import parsing;
import reporting;

using namespace std::literals;

class ProgramBuilder;
struct BuiltinBlockInitializer;

namespace sem {

struct Label {
    const char *defining_occurrence;
    const char *prefixing_occurrence;
};

export
class DynamicType {
public:
    using ptr_t = std::shared_ptr<const DynamicType>;

    DynamicType() = default;

    DynamicType(const DynamicType &) = delete;
    DynamicType &operator =(const DynamicType &) = delete;

    virtual constexpr
    ~DynamicType() = default;

    virtual std::string
    str() const = 0;

    virtual bool
    canBeFileComponent() const { return true; }

    virtual const DynamicType &
    fullRange() const { return *this; }
};

export
class Type : public DynamicType {
public:
    using ptr_t = std::shared_ptr<const Type>;
};

export
class TypeOrdinal : public Type {
public:
    using ptr_t = std::shared_ptr<const TypeOrdinal>;

    bool
    isCompatibleWith(const TypeOrdinal &other) const {
        return &fullRange() == &other.fullRange();
    }

    const TypeOrdinal &
    fullRange() const override { return *this; }

    virtual pascal_integer_t
    smallestOrdinal() const = 0;

    virtual pascal_integer_t
    largestOrdinal() const = 0;
};

export
class Constant {
public:
    using ptr_t = std::shared_ptr<const Constant>;

    virtual constexpr
    ~Constant() = default;

    virtual const Type &
    type() const = 0;
};

export
class ConstantOrdinal : public Constant {
public:
    using ptr_t = std::shared_ptr<const ConstantOrdinal>;

    const TypeOrdinal &
    type() const override = 0;

    virtual std::string
    str() const = 0;

    virtual pascal_integer_t
    ordinalNumber() const = 0;
};

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
        // so `smallest_value_->type()` should be sufficient, but
        // just in case, we also call `fullRange` on that.
        return smallest_value_->type().fullRange();
    }

    pascal_integer_t
    smallestOrdinal() const override {
        return smallest_value_->ordinalNumber();
    }

    pascal_integer_t
    largestOrdinal() const override {
        return largest_value_->ordinalNumber();
    }

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

private:
    TypeOrdinal::ptr_t index_type_;
    Type::ptr_t component_type_;
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

private:
    Type::ptr_t component_type_;
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
    explicit VariantPart(TypeOrdinal::ptr_t tag_type)
        : tag_type_(tag_type)
    {}

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

    std::vector<std::string>
    allFieldNames() const;

    void
    addField(const std::string &name, Type::ptr_t type) {
        field_names_.push_back(name);
        field_types_.emplace(name, type);
    }

    const std::optional<VariantPart> &
    variantPart() const { return variant_part_; }

    void
    setVariantPart(const VariantPart &variant_part) {
        variant_part_ = variant_part;
    }

    bool
    canBeFileComponent() const;

private:
    std::vector<std::string> field_names_;
    std::unordered_map<std::string, Type::ptr_t> field_types_;
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
    ) : base_type_(base_type), is_packed_(is_packed) {}

    std::string
    str() const override {
        return (is_packed_ ? "packed "s : ""s)
            + "set of "s + base_type_->str();
    }

private:
    TypeOrdinal::ptr_t base_type_;
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

private:
    std::string smallest_bound_;
    std::string largest_bound_;
    TypeOrdinal::ptr_t bound_type_;
    DynamicType::ptr_t component_type_;
    bool is_packed_;
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
            Type::ptr_t(std::shared_ptr<void>(), &TypeChar::instance()),
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

class Scope;

class VariableAccess {
public:
    VariableAccess(const std::string &id, std::size_t scope_index)
        : id_(id), scope_index_(scope_index) {}

    virtual ~VariableAccess() = default;

    const std::string &
    id() const { return id_; }

    std::size_t
    scopeIndex() const { return scope_index_; }

    virtual DynamicType::ptr_t
    type(Scope &scope) const = 0;

private:
    std::string id_;
    std::size_t scope_index_;
};

class VariableAccessActivationResult : public VariableAccess {
public:
    using VariableAccess::VariableAccess;

    DynamicType::ptr_t
    type(Scope &scope) const override;
};

class VariableAccessEntire : public VariableAccess {
public:
    using VariableAccess::VariableAccess;

    DynamicType::ptr_t
    type(Scope &scope) const override;
};

class Statement {
public:
    virtual ~Statement() = default;
};

class StatementAssignment : public Statement {
public:
    StatementAssignment(std::unique_ptr<VariableAccess> &&access)
        : access_(std::move(access)) {}

private:
    std::unique_ptr<VariableAccess> access_;
};

class CaseListElement {
public:
    CaseListElement(
        std::span<ConstantOrdinal::ptr_t> constants,
        std::unique_ptr<Statement> &&statement
    )
        : constants_(constants.begin(), constants.end())
        , statement_(std::move(statement))
    {}

private:
    std::vector<ConstantOrdinal::ptr_t> constants_;
    std::unique_ptr<Statement> statement_;
};

class StatementCase : public Statement {
public:
    explicit
    StatementCase(std::vector<CaseListElement> &&cases)
        : cases_(std::move(cases)) {}

private:
    std::vector<CaseListElement> cases_;
};

class StatementCompound : public Statement {
public:
    explicit
    StatementCompound(std::vector<std::unique_ptr<Statement>> &&statements)
        : statements_(std::move(statements)) {}

private:
    std::vector<std::unique_ptr<Statement>> statements_;
};

class StatementEmpty : public Statement {
};

class StatementFor : public Statement {
public:
    StatementFor(
        const std::string &control_variable,
        nodes::RangeDirection direction,
        std::unique_ptr<Statement> &&body
    )
        : control_variable_(control_variable)
        , direction_(direction)
        , body_(std::move(body))
    {}

private:
    std::string control_variable_;
    nodes::RangeDirection direction_;
    std::unique_ptr<Statement> body_;
};

class StatementGoto : public Statement {
public:
    StatementGoto(
        pascal_integer_t label,
        std::size_t scope_index
    ) : label_(label), scope_index_(scope_index) {}

private:
    pascal_integer_t label_;
    std::size_t scope_index_;
};

class StatementIf : public Statement {
public:
    StatementIf(
        std::unique_ptr<Statement> &&true_branch,
        std::unique_ptr<Statement> &&false_branch
    )
        : true_branch_(std::move(true_branch))
        , false_branch_(std::move(false_branch))
    {}

private:
    std::unique_ptr<Statement> true_branch_;
    std::unique_ptr<Statement> false_branch_;
};

class StatementLabeled : public Statement {
public:
    StatementLabeled(pascal_integer_t label, std::unique_ptr<Statement> &&unlabeled)
        : label_(label), unlabeled_(std::move(unlabeled)) {}

private:
    pascal_integer_t label_;
    std::unique_ptr<Statement> unlabeled_;
};

class StatementRepeat : public Statement {
public:
    StatementRepeat(std::vector<std::unique_ptr<Statement>> &&statements)
        : statements_(std::move(statements)) {}

private:
    std::vector<std::unique_ptr<Statement>> statements_;
};

class StatementWhile : public Statement {
public:
    StatementWhile(
        std::unique_ptr<Statement> &&body
    )
        : body_(std::move(body))
    {}

private:
    std::unique_ptr<Statement> body_;
};

class StatementWith;

struct DefiningOccurrence {
    const char *location;
    enum Kind { NOT_TYPE, TYPE } kind;
};

class Scope {
public:
    struct LookupResult {
        std::size_t scope_index;
        Scope *scope;
        DefiningOccurrence defining_occurrence;
    };

    using region_t = std::variant<std::monostate, Block *, StatementWith *>;

    Scope(Scope *parent, const region_t &region = region_t())
        : parent_(parent), region_(region) {}

    void
    add(
        const std::string &id,
        const char *location,
        DefiningOccurrence::Kind kind = DefiningOccurrence::NOT_TYPE
    ) {
        // Ignore duplicate identifiers; this is so that when the analysis
        // logic reports the error, it can note the first defining occurrence.
        dos_.try_emplace(id, DefiningOccurrence{location, kind});
    }

    void
    add(
        const nodes::Identifier &id_node,
        DefiningOccurrence::Kind kind = DefiningOccurrence::NOT_TYPE
    ) {
        add(id_node.spelling, id_node.view.data(), kind);
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

    void
    mergeFrom(const Scope &s) {
        dos_.insert(s.dos_.begin(), s.dos_.end());
    }

    Scope *
    parent() { return parent_; }

    Scope &
    parent(std::size_t index) {
        if (index == 0) return *this;
        assert(parent_);
        return parent_->parent(index - 1);
    }

    Block *
    block() {
        Block **pb = std::get_if<Block *>(&region_);
        return pb ? *pb : nullptr;
    }

    Block &
    closestContainingBlock() {
        if (auto *b = block()) return *b;

        // at least one scope in the chain has to be associated with a block
        assert(parent_);

        return parent_->closestContainingBlock();
    }

    std::optional<LookupResult>
    lookup(const std::string &id) {
        std::size_t scope_index = 0;

        for (
            auto *lookup_scope = this;
            lookup_scope;
            lookup_scope = lookup_scope->parent(), ++scope_index
        ) {
            auto it = lookup_scope->dos_.find(id);
            if (it != lookup_scope->dos_.end())
                return LookupResult{scope_index, lookup_scope, it->second};
        }

        return std::nullopt;
    }

    bool
    containsShallow(const std::string &id) const {
        return dos_.contains(id);
    }

    DefiningOccurrence
    lookupShallowUnsafe(const std::string &id) const {
        return dos_.at(id);
    }

private:
    Scope *parent_;
    region_t region_;

    std::unordered_map<std::string, DefiningOccurrence> dos_;
};

class StatementWith : public Statement {
public:
    StatementWith(
        Scope &parent_scope,
        std::unique_ptr<VariableAccess> &&variable
    )
        : scope_(&parent_scope, this), variable_(std::move(variable))
    {}

    Scope &
    scope() { return scope_; }

    void
    setBody(std::unique_ptr<Statement> &&body) { body_ = std::move(body); }

private:
    Scope scope_;
    std::unique_ptr<VariableAccess> variable_;
    std::unique_ptr<Statement> body_;
};

class Subroutine;

export
class Block {
public:
    Block(Block *parent_block)
        : scope_(parent_block ? &parent_block->scope_ : nullptr, this)
    {}

    // Copying a block trivially would mess up the parent scope pointers
    // in the subroutines.
    Block(const Block &) = delete;
    Block &operator =(const Block &) = delete;

    const Scope &
    scope() const { return scope_; }

    Type::ptr_t
    variableType(const std::string &name) const { return variables_.at(name); }

    const Subroutine &
    subroutine(const std::string &name) const { return subroutines_.at(name); }

private:
    Scope scope_;

    std::unordered_map<pascal_integer_t, Label> labels_;
    std::unordered_map<std::string, Constant::ptr_t> constants_;
    std::unordered_map<std::string, Type::ptr_t> types_;
    std::unordered_map<std::string, Type::ptr_t> variables_;
    std::unordered_map<std::string, Subroutine> subroutines_;

    std::unique_ptr<StatementCompound> statement_;

    friend class ProgramBuilder;
    friend struct BuiltinBlockInitializer;
};

struct FormalParameterSection;

export
class Signature {
public:
    Signature(
        std::span<FormalParameterSection> parameters,
        Type::ptr_t result_type
    )
        : parameters_(parameters.begin(), parameters.end())
        , result_type_(result_type)
    {}

    Type::ptr_t
    resultType() const { return result_type_; }

private:
    std::vector<FormalParameterSection> parameters_;
    Type::ptr_t result_type_;
};

export
class RegularParameterSection {
public:
    RegularParameterSection(
        bool is_variable,
        std::span<std::string> names,
        DynamicType::ptr_t type
    )
        : is_variable_(is_variable)
        , names_(names.begin(), names.end())
        , type_(type)
    {
        assert(!names.empty());
    }

private:
    bool is_variable_;

    // It would have made more sense to have one parameter object per name,
    // but the Pascal signature matching rules require the parameter sections
    // to match, so we have to remember which names were originally in which
    // sections.
    std::vector<std::string> names_;
    DynamicType::ptr_t type_;
};

export
class SubroutineParameterSpecification {
public:
    SubroutineParameterSpecification(
        const std::string &name, const Signature &signature
    ) : name_(name), signature_(signature) {}

private:
    std::string name_;
    Signature signature_;
};

// Ugly, but we can't just alias FormalParameterSection to std::variant,
// since we need to forward-declare it to break the dependency loop.
struct FormalParameterSection
{
    std::variant<RegularParameterSection, SubroutineParameterSpecification> v;
};

export
class Subroutine {
public:
    Subroutine(
        const char *declaration_location,
        const Signature &signature,
        Block &parent_block
    )
        : last_declaration_location_(declaration_location)
        , signature_(signature)
        , block_(&parent_block)
    {}

    const Signature &
    signature() const { return signature_; }

    const Block &
    block() const { return block_; }

private:
    const char *last_declaration_location_;

    Signature signature_;
    Block block_;

    friend class ProgramBuilder;
};

export
class Program {
public:
    Program();

private:
    std::unordered_map<std::string, const char *> parameters_;
    Block block_;

    friend class ProgramBuilder;
};

}

export
std::unique_ptr<sem::Program>
analyze(const nodes::Program &program_node, Reporter &reporter);
