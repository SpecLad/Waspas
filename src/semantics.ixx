// SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
//
// SPDX-License-Identifier: MPL-2.0

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

export import :builtins;
export import :constants;
export import :core;
export import :expressions;
export import :statements;
export import :types;

import parsing;
import reporting;
import utilities;

using namespace std::literals;

class Builder;
class ProgramBuilder;
class StatementBuilder;
struct BuiltinBlockInitializer;

namespace sem {

export class Program;
export class Subroutine;

export
class Block {
public:
    using container_t = std::variant<std::monostate, Program *, Subroutine *>;

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

    const Program *
    containingProgram() const { return container<Program>(); }

    const Subroutine *
    containingSubroutine() const { return container<Subroutine>(); }

    Type::ptr_t
    type(const Cisref &name) const { return types_.at(name); }

    Type::ptr_t
    variableType(const Cisref &name) const { return variables_.at(name).type; }

    bool
    hasSubroutine(const Cisref &name) const { return subroutines_.contains(name); }

    Subroutine &
    subroutine(const Cisref &name) { return subroutines_.at(name); }

    const Subroutine &
    subroutine(const Cisref &name) const { return subroutines_.at(name); }

private:
    template <typename T>
    const T *
    container() const {
        auto *c = std::get_if<T *>(&container_);
        return c ? *c : nullptr;
    }

    Scope scope_;
    container_t container_;

    struct Label {
        const char *defining_occurrence;
        const char *prefixing_occurrence;
    };

    struct Variable {
        Type::ptr_t type;
        const char *subroutine_threat_location = nullptr;
    };

    std::unordered_map<pascal_integer_t, Label> labels_;
    std::unordered_map<Cisref, Constant::ptr_t> constants_;
    std::unordered_map<Cisref, Type::ptr_t> types_;
    std::unordered_map<Cisref, Variable> variables_;
    std::unordered_map<Cisref, Subroutine> subroutines_;
    std::unordered_map<Cisref, builtin_function_resolve_f> builtin_functions_;
    std::unordered_map<Cisref, builtin_procedure_resolve_f> builtin_procedures_;

    std::unique_ptr<StatementCompound> statement_;

    friend class ::Builder;
    friend class ::ProgramBuilder;
    friend class ::StatementBuilder;
    friend struct ::BuiltinBlockInitializer;
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
    hasRegularParameter(const Cisref &name) const {
        return regular_parameter_types_.contains(name);
    }

    Type::ptr_t
    regularParameterType(const Cisref &name) const {
        return regular_parameter_types_.at(name);
    }

    bool
    hasBound(const Cisref &name) const {
        return bound_types_.contains(name);
    }

    TypeOrdinal::ptr_t
    boundType(const Cisref &name) const {
        return bound_types_.at(name);
    }

    bool
    hasSubroutineParameter(const Cisref &name) const {
        return subroutine_parameter_signatures_.contains(name);
    }

    const Signature &
    subroutineParameterSignature(const Cisref &name) const {
        return *subroutine_parameter_signatures_.at(name);
    }

    const std::vector<FormalParameterSection> &
    parameters() const { return parameters_; }

    bool
    isCongruousWith(const Signature &other) const;

private:
    std::vector<FormalParameterSection> parameters_;
    std::unordered_map<Cisref, Type::ptr_t> regular_parameter_types_;
    std::unordered_map<Cisref, TypeOrdinal::ptr_t> bound_types_;
    std::unordered_map<Cisref, const Signature *> subroutine_parameter_signatures_;
    Type::ptr_t result_type_;
};

export
class RegularParameterSection {
public:
    RegularParameterSection(
        bool is_variable,
        std::span<Cisref> names,
        Type::ptr_t type
    )
        : is_variable_(is_variable)
        , names_(names.begin(), names.end())
        , type_(type)
    {
        assert(!names.empty());
    }

    bool
    isVariable() const { return is_variable_; }

    const std::vector<Cisref> &
    names() const { return names_; }

    Type::ptr_t
    type() const { return type_; }

private:
    bool is_variable_;

    // It would have made more sense to have one parameter object per name,
    // but the Pascal signature matching rules require the parameter sections
    // to match, so we have to remember which names were originally in which
    // sections.
    std::vector<Cisref> names_;
    Type::ptr_t type_;
};

export
class SubroutineParameterSpecification {
public:
    SubroutineParameterSpecification(
        const Cisref &name, const Signature &signature
    ) : name_(name), signature_(signature) {}

    Cisref
    name() const { return name_; }

    const Signature &
    signature() const { return signature_; }

private:
    Cisref name_;
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

    friend class ::ProgramBuilder;
    friend class ::StatementBuilder;
};

export
class Program {
public:
    Program();

private:
    std::unordered_map<Cisref, const char *> parameters_;
    Block block_;

    friend class ::ProgramBuilder;
    friend class ::StatementBuilder;
};

}

export
std::unique_ptr<sem::Program>
analyze(const nodes::Program &program_node, Reporter &reporter);
