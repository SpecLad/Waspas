module;

#include <memory>
#include <span>
#include <string>
#include <vector>

export module semantics:statements;

export import :expressions;
export import :types;

import parsing;

namespace sem {

export
class Statement {
public:
    virtual ~Statement() = default;
};

export
class StatementAssignment final : public Statement {
public:
    StatementAssignment(
        std::unique_ptr<VariableAccess> &&access,
        std::unique_ptr<Expression> &&expression
    ) : access_(std::move(access)), expression_(std::move(expression)) {}

private:
    std::unique_ptr<VariableAccess> access_;
    std::unique_ptr<Expression> expression_;
};

export
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

export
class StatementCase final : public Statement {
public:
    explicit
    StatementCase(std::vector<CaseListElement> &&cases)
        : cases_(std::move(cases)) {}

private:
    std::vector<CaseListElement> cases_;
};

export
class StatementCompound final : public Statement {
public:
    explicit
    StatementCompound(std::vector<std::unique_ptr<Statement>> &&statements)
        : statements_(std::move(statements)) {}

private:
    std::vector<std::unique_ptr<Statement>> statements_;
};

export
class StatementEmpty final : public Statement {
};

export
class StatementFor final : public Statement {
public:
    StatementFor(
        const Cisref &control_variable,
        std::unique_ptr<Expression> &&initial_value,
        nodes::RangeDirection direction,
        std::unique_ptr<Expression> &&final_value,
        std::unique_ptr<Statement> &&body
    )
        : control_variable_(control_variable)
        , initial_value_(std::move(initial_value))
        , direction_(direction)
        , final_value_(std::move(final_value))
        , body_(std::move(body))
    {}

private:
    Cisref control_variable_;
    std::unique_ptr<Expression> initial_value_;
    nodes::RangeDirection direction_;
    std::unique_ptr<Expression> final_value_;
    std::unique_ptr<Statement> body_;
};

export
class StatementGoto final : public Statement {
public:
    StatementGoto(
        pascal_integer_t label,
        std::size_t scope_index
    ) : label_(label), scope_index_(scope_index) {}

private:
    pascal_integer_t label_;
    std::size_t scope_index_;
};

export
class StatementIf final : public Statement {
public:
    StatementIf(
        std::unique_ptr<Expression> &&condition,
        std::unique_ptr<Statement> &&true_branch,
        std::unique_ptr<Statement> &&false_branch
    )
        : condition_(std::move(condition))
        , true_branch_(std::move(true_branch))
        , false_branch_(std::move(false_branch))
    {}

private:
    std::unique_ptr<Expression> condition_;
    std::unique_ptr<Statement> true_branch_;
    std::unique_ptr<Statement> false_branch_;
};

export
class StatementLabeled final : public Statement {
public:
    StatementLabeled(pascal_integer_t label, std::unique_ptr<Statement> &&unlabeled)
        : label_(label), unlabeled_(std::move(unlabeled)) {}

private:
    pascal_integer_t label_;
    std::unique_ptr<Statement> unlabeled_;
};

export
class StatementProcedure final : public Statement {
public:
    StatementProcedure(
        const SubroutineReference &reference,
        std::vector<sem::actual_parameter_section_t> &&actual_parameters
    )
        : reference_(reference)
        , actual_parameters_(std::move(actual_parameters))
    {}

private:
    SubroutineReference reference_;
    std::vector<sem::actual_parameter_section_t> actual_parameters_;
};

export
class StatementRepeat final : public Statement {
public:
    StatementRepeat(
        std::vector<std::unique_ptr<Statement>> &&statements,
        std::unique_ptr<Expression> &&condition
    )
        : statements_(std::move(statements))
        , condition_(std::move(condition))
    {}

private:
    std::vector<std::unique_ptr<Statement>> statements_;
    std::unique_ptr<Expression> condition_;
};

export
class StatementWhile final : public Statement {
public:
    StatementWhile(
        std::unique_ptr<Expression> &&condition,
        std::unique_ptr<Statement> &&body
    )
        : condition_(std::move(condition)), body_(std::move(body))
    {}

private:
    std::unique_ptr<Expression> condition_;
    std::unique_ptr<Statement> body_;
};

export
class StatementWith : public Statement {
public:
    StatementWith(
        Scope &parent_scope,
        std::unique_ptr<VariableAccess> &&variable
    )
        : scope_(&parent_scope, this)
        , variable_(std::move(variable))
        , variable_type_(dynamic_cast<const TypeRecord &>(
            variable_->variableType(parent_scope)))
    {}

    Scope &
    scope() { return scope_; }

    const TypeRecord &
    variableType() const { return variable_type_; }

    void
    setBody(std::unique_ptr<Statement> &&body) { body_ = std::move(body); }

private:
    Scope scope_;
    std::unique_ptr<VariableAccess> variable_;
    const TypeRecord &variable_type_;
    std::unique_ptr<Statement> body_;
};

}
