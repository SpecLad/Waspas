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

export import :constants;
export import :core;
export import :expressions;
export import :statements;
export import :types;

import parsing;
import reporting;
import utilities;

using namespace std::literals;

class ProgramBuilder;
struct BuiltinBlockInitializer;

namespace sem {

struct Label {
    const char *defining_occurrence;
    const char *prefixing_occurrence;
};

class Subroutine;

export
class Block {
public:
    // This is a variant rather than an optional,
    // because we'll likely add Program * as an alternative later.
    using container_t = std::variant<std::monostate, Subroutine *>;

    explicit
    Block(Block *parent_block, const container_t &container = {})
        : scope_(parent_block ? &parent_block->scope_ : nullptr, this)
        , container_(container)
    {}

    // Copying a block trivially would mess up the parent scope pointers
    // in the subroutines.
    Block(const Block &) = delete;
    Block &operator =(const Block &) = delete;

    const Scope &
    scope() const { return scope_; }

    const Subroutine *
    containingSubroutine() const {
        auto *s = std::get_if<Subroutine *>(&container_);
        return s ? *s : nullptr;
    }

    Type::ptr_t
    type(const std::string &name) const { return types_.at(name); }

    Type::ptr_t
    variableType(const std::string &name) const { return variables_.at(name).type; }

    Subroutine &
    subroutine(const std::string &name) { return subroutines_.at(name); }

    const Subroutine &
    subroutine(const std::string &name) const { return subroutines_.at(name); }

private:
    Scope scope_;
    container_t container_;

    struct Variable {
        Type::ptr_t type;
        const char *subroutine_threat_location = nullptr;
    };

    std::unordered_map<pascal_integer_t, Label> labels_;
    std::unordered_map<std::string, Constant::ptr_t> constants_;
    std::unordered_map<std::string, Type::ptr_t> types_;
    std::unordered_map<std::string, Variable> variables_;
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
    );

    Type::ptr_t
    resultType() const { return result_type_; }

    bool
    hasRegularParameter(const std::string &name) const {
        return regular_parameter_types_.contains(name);
    }

    DynamicType::ptr_t
    regularParameterType(const std::string &name) const {
        return regular_parameter_types_.at(name);
    }

    bool
    hasBound(const std::string &name) const {
        return bound_types_.contains(name);
    }

    TypeOrdinal::ptr_t
    boundType(const std::string &name) const {
        return bound_types_.at(name);
    }

private:
    std::vector<FormalParameterSection> parameters_;
    std::unordered_map<std::string, DynamicType::ptr_t> regular_parameter_types_;
    std::unordered_map<std::string, TypeOrdinal::ptr_t> bound_types_;
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

    const std::vector<std::string> &
    names() const { return names_; }

    DynamicType::ptr_t
    type() const { return type_; }

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
        , block_(&parent_block, this)
        , contains_result_assignment_(false)
    {}

    const Signature &
    signature() const { return signature_; }

    const Block &
    block() const { return block_; }

private:
    const char *last_declaration_location_;

    Signature signature_;
    Block block_;

    bool contains_result_assignment_;

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
