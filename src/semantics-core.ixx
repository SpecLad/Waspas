module;

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <unordered_map>

export module semantics:core;

import parsing;

namespace sem {

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

    // For a type T, returns the type that an expression of type T should
    // be treated as having, according to Pascal rules.
    virtual const DynamicType &
    promoted() const { return *this; }

    virtual bool
    isCompatibleWith(const DynamicType &other) const {
        return this == &other;
    }

    virtual bool
    isAssignmentCompatibleWith(const DynamicType &other) const {
        if (this == &other) return canBeFileComponent();
        return isCompatibleWith(other);
    }
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
    isCompatibleWith(const DynamicType &other) const override {
        if (auto *other_ordinal = dynamic_cast<const TypeOrdinal *>(&other))
            return &fullRange() == &other_ordinal->fullRange();
        return Type::isCompatibleWith(other);
    }

    const TypeOrdinal &
    promoted() const override { return fullRange(); }

    virtual const TypeOrdinal &
    fullRange() const { return *this; }

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

    virtual Type::ptr_t
    type() const = 0;
};

export
class ConstantOrdinal : public Constant {
public:
    using ptr_t = std::shared_ptr<const ConstantOrdinal>;

    Type::ptr_t
    type() const override final { return typeOrdinal(); }

    virtual TypeOrdinal::ptr_t
    typeOrdinal() const = 0;

    virtual std::string
    str() const = 0;

    virtual pascal_integer_t
    ordinalNumber() const = 0;
};

struct DefiningOccurrence {
    const char *location;
    enum Kind { NOT_TYPE, TYPE } kind;
};

export
class Block;

export
class StatementWith;

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

    const Scope *
    parent() const { return parent_; }

    Scope &
    parent(std::size_t index) {
        if (index == 0) return *this;
        assert(parent_);
        return parent_->parent(index - 1);
    }

    const Scope &
    parent(std::size_t index) const {
        if (index == 0) return *this;
        assert(parent_);
        return parent_->parent(index - 1);
    }

    Block *
    block() { return region<Block>(); }

    const Block *
    block() const { return region<Block>(); }

    const StatementWith *
    statementWith() const { return region<StatementWith>(); }

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
    template <typename T>
    T *
    region() const {
        T *const *p = std::get_if<T *>(&region_);
        return p ? *p : nullptr;
    }

    Scope *parent_;
    region_t region_;

    std::unordered_map<std::string, DefiningOccurrence> dos_;
};

}
