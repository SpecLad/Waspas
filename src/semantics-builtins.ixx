module;

#include <memory>
#include <span>
#include <string_view>
#include <vector>

export module semantics:builtins;

export import :expressions;
export import :statements;

namespace sem {

export
class ExpressionFunctionSingleParameter : public Expression {
public:
    ExpressionFunctionSingleParameter(std::unique_ptr<sem::Expression> &&parameter)
        : parameter_(std::move(parameter)) {}

    const sem::Expression &parameter() const { return *parameter_; }

private:
    std::unique_ptr<sem::Expression> parameter_;
};

export
class ExpressionFunctionAbsLike : public ExpressionFunctionSingleParameter {
public:
    using ExpressionFunctionSingleParameter::ExpressionFunctionSingleParameter;

    const sem::Type &valueType(const Scope &scope) const override {
        return parameter().valueType(scope);
    }
};

export
class ExpressionFunctionAbs : public ExpressionFunctionAbsLike {
public:
    using ExpressionFunctionAbsLike::ExpressionFunctionAbsLike;
};

export
class ExpressionFunctionPred : public ExpressionFunctionAbsLike {
public:
    using ExpressionFunctionAbsLike::ExpressionFunctionAbsLike;
};

export
class ExpressionFunctionSqr : public ExpressionFunctionAbsLike {
public:
    using ExpressionFunctionAbsLike::ExpressionFunctionAbsLike;
};

export
class ExpressionFunctionSucc : public ExpressionFunctionAbsLike {
public:
    using ExpressionFunctionAbsLike::ExpressionFunctionAbsLike;
};

export
class ExpressionFunctionExpLike : public ExpressionFunctionSingleParameter {
public:
    using ExpressionFunctionSingleParameter::ExpressionFunctionSingleParameter;

    const sem::Type &valueType(const Scope &) const override {
        return sem::TypeReal::instance();
    }
};

export
class ExpressionFunctionArctan : public ExpressionFunctionExpLike {
public:
    using ExpressionFunctionExpLike::ExpressionFunctionExpLike;
};

export
class ExpressionFunctionCos : public ExpressionFunctionExpLike {
public:
    using ExpressionFunctionExpLike::ExpressionFunctionExpLike;
};

export
class ExpressionFunctionExp : public ExpressionFunctionExpLike {
public:
    using ExpressionFunctionExpLike::ExpressionFunctionExpLike;
};

export
class ExpressionFunctionLn : public ExpressionFunctionExpLike {
public:
    using ExpressionFunctionExpLike::ExpressionFunctionExpLike;
};

export
class ExpressionFunctionSin : public ExpressionFunctionExpLike {
public:
    using ExpressionFunctionExpLike::ExpressionFunctionExpLike;
};

export
class ExpressionFunctionSqrt : public ExpressionFunctionExpLike {
public:
    using ExpressionFunctionExpLike::ExpressionFunctionExpLike;
};

export
class ExpressionFunctionChr : public ExpressionFunctionSingleParameter {
public:
    using ExpressionFunctionSingleParameter::ExpressionFunctionSingleParameter;

    const sem::Type &valueType(const Scope &) const override {
        return sem::TypeChar::instance();
    }
};

export
class ExpressionFunctionEofLike : public Expression {
public:
    ExpressionFunctionEofLike(std::unique_ptr<sem::VariableAccess> &&file)
        : file_(std::move(file)) {}

    const sem::Type &valueType(const Scope &) const override {
        return sem::TypeBoolean::instance();
    }

private:
    std::unique_ptr<sem::Expression> file_;
};

export
class ExpressionFunctionEof : public ExpressionFunctionEofLike {
public:
    using ExpressionFunctionEofLike::ExpressionFunctionEofLike;
};

export
class ExpressionFunctionEoln : public ExpressionFunctionEofLike {
public:
    using ExpressionFunctionEofLike::ExpressionFunctionEofLike;
};

export
class ExpressionFunctionOdd : public ExpressionFunctionSingleParameter {
public:
    using ExpressionFunctionSingleParameter::ExpressionFunctionSingleParameter;

    const sem::Type &valueType(const Scope &) const override {
        return sem::TypeBoolean::instance();
    }
};

export
class ExpressionFunctionOrdLike : public ExpressionFunctionSingleParameter {
public:
    using ExpressionFunctionSingleParameter::ExpressionFunctionSingleParameter;

    const sem::Type &valueType(const Scope &) const override {
        return sem::TypeInteger::instance();
    }
};

export
class ExpressionFunctionOrd : public ExpressionFunctionOrdLike {
public:
    using ExpressionFunctionOrdLike::ExpressionFunctionOrdLike;
};

export
class StatementProcedureGetLike : public Statement {
public:
    explicit
    StatementProcedureGetLike(
        std::unique_ptr<sem::VariableAccess> &&file
    ) : file_(std::move(file)) {}

private:
    std::unique_ptr<sem::VariableAccess> file_;
};

export
class StatementProcedureGet : public StatementProcedureGetLike {
public:
    using StatementProcedureGetLike::StatementProcedureGetLike;
};

export
class StatementProcedureReset : public StatementProcedureGetLike {
public:
    using StatementProcedureGetLike::StatementProcedureGetLike;
};

export
class StatementProcedureRewrite : public StatementProcedureGetLike {
public:
    using StatementProcedureGetLike::StatementProcedureGetLike;
};

export
class StatementProcedurePage: public StatementProcedureGetLike {
public:
    using StatementProcedureGetLike::StatementProcedureGetLike;
};

export
class StatementProcedurePut : public StatementProcedureGetLike {
public:
    using StatementProcedureGetLike::StatementProcedureGetLike;
};

export
class StatementProcedureNewLike : public Statement {
public:
    StatementProcedureNewLike(
        std::unique_ptr<sem::VariableAccess> &&pointer,
        std::span<ConstantOrdinal::ptr_t> case_constants
    )
        : pointer_(std::move(pointer))
        , case_constants_(case_constants.begin(), case_constants.end())
    {}

private:
    std::unique_ptr<sem::VariableAccess> pointer_;
    std::vector<ConstantOrdinal::ptr_t> case_constants_;
};

export
class StatementProcedureDispose : public StatementProcedureNewLike {
public:
    using StatementProcedureNewLike::StatementProcedureNewLike;
};

export
class StatementProcedureNew : public StatementProcedureNewLike {
public:
    using StatementProcedureNewLike::StatementProcedureNewLike;
};

export
class StatementProcedurePack : public Statement {
public:
    StatementProcedurePack(
        std::unique_ptr<sem::VariableAccess> &&source,
        std::unique_ptr<sem::Expression> &&start_index,
        std::unique_ptr<sem::VariableAccess> &&destination
    )
        : source_(std::move(source))
        , start_index_(std::move(start_index))
        , destination_(std::move(destination))
    {}

private:
    std::unique_ptr<sem::VariableAccess> source_;
    std::unique_ptr<sem::Expression> start_index_;
    std::unique_ptr<sem::VariableAccess> destination_;
};

export
class StatementProcedureReadLike : public Statement {
public:
    StatementProcedureReadLike(
        std::unique_ptr<sem::VariableAccess> &&file,
        std::vector<std::unique_ptr<sem::VariableAccess>> &&variables
    ) : file_(std::move(file)), variables_(std::move(variables)) {}

private:
    std::unique_ptr<sem::VariableAccess> file_;
    std::vector<std::unique_ptr<sem::VariableAccess>> variables_;
};

export
class StatementProcedureReadText : public StatementProcedureReadLike {
public:
    using StatementProcedureReadLike::StatementProcedureReadLike;
};

export
class StatementProcedureReadTyped : public StatementProcedureReadLike {
public:
    using StatementProcedureReadLike::StatementProcedureReadLike;
};

export
class StatementProcedureReadln : public StatementProcedureReadLike {
public:
    using StatementProcedureReadLike::StatementProcedureReadLike;
};

export
class StatementProcedureUnpack : public Statement {
public:
    StatementProcedureUnpack(
        std::unique_ptr<sem::VariableAccess> &&source,
        std::unique_ptr<sem::VariableAccess> &&destination,
        std::unique_ptr<sem::Expression> &&start_index
    )
        : source_(std::move(source))
        , destination_(std::move(destination))
        , start_index_(std::move(start_index))
    {}

private:
    std::unique_ptr<sem::VariableAccess> source_;
    std::unique_ptr<sem::VariableAccess> destination_;
    std::unique_ptr<sem::Expression> start_index_;
};

export
class WriteParameter {
public:
    explicit
    WriteParameter(
        std::unique_ptr<sem::Expression> &&value,
        std::unique_ptr<sem::Expression> &&total_width = nullptr,
        std::unique_ptr<sem::Expression> &&frac_digits = nullptr
    )
        : value_(std::move(value))
        , total_width_(std::move(total_width))
        , frac_digits_(std::move(frac_digits))
    {}

private:
    std::unique_ptr<sem::Expression> value_;
    std::unique_ptr<sem::Expression> total_width_;
    std::unique_ptr<sem::Expression> frac_digits_;
};

export
class StatementProcedureWriteTextLike : public Statement {
public:
    StatementProcedureWriteTextLike(
        std::unique_ptr<sem::VariableAccess> &&file,
        std::vector<WriteParameter> &&parameters
    ) : file_(std::move(file)), parameters_(std::move(parameters)) {}

private:
    std::unique_ptr<sem::VariableAccess> file_;
    std::vector<WriteParameter> parameters_;
};

export
class StatementProcedureWriteText : public StatementProcedureWriteTextLike {
public:
    using StatementProcedureWriteTextLike::StatementProcedureWriteTextLike;
};

export
class StatementProcedureWriteTyped : public Statement {
public:
    StatementProcedureWriteTyped(
        std::unique_ptr<sem::VariableAccess> &&file,
        std::vector<std::unique_ptr<sem::Expression>> &&values
    ) : file_(std::move(file)), values_(std::move(values)) {}

private:
    std::unique_ptr<sem::VariableAccess> file_;
    std::vector<std::unique_ptr<sem::Expression>> values_;
};

export
class StatementProcedureWriteln : public StatementProcedureWriteTextLike {
public:
    using StatementProcedureWriteTextLike::StatementProcedureWriteTextLike;
};

}

class StatementBuilder;

using builtin_function_resolve_f
    = std::unique_ptr<sem::Expression>(StatementBuilder:: *)(
        sem::Scope &scope,
        std::span<const nodes::ActualParameter> actual_parameter_nodes,
        const char *actual_parameter_end_location
    );

extern const std::initializer_list<std::pair<const std::string_view, builtin_function_resolve_f>>
    BUILTIN_FUNCTIONS;

using builtin_procedure_resolve_f
    = std::unique_ptr<sem::Statement>(StatementBuilder:: *)(
        sem::Scope &scope,
        std::span<const nodes::ActualParameter> actual_parameter_nodes,
        const char *actual_parameter_end_location
    );

extern const std::initializer_list<std::pair<const std::string_view, builtin_procedure_resolve_f>>
    BUILTIN_PROCEDURES;
