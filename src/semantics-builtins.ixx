module;

#include <memory>
#include <vector>

export module semantics:builtins;

export import :expressions;
export import :statements;

namespace sem {

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
class StatementProcedurePut : public StatementProcedureGetLike {
public:
    using StatementProcedureGetLike::StatementProcedureGetLike;
};

export
class StatementProcedureReadText : public Statement {
public:
    StatementProcedureReadText(
        std::unique_ptr<sem::VariableAccess> &&file,
        std::vector<std::unique_ptr<sem::VariableAccess>> &&variables
    ) : file_(std::move(file)), variables_(std::move(variables)) {}

private:
    std::unique_ptr<sem::VariableAccess> file_;
    std::vector<std::unique_ptr<sem::VariableAccess>> variables_;
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
class StatementProcedureWriteText : public Statement {
public:
    StatementProcedureWriteText(
        std::unique_ptr<sem::VariableAccess> &&file,
        std::vector<WriteParameter> &&parameters
    ) : file_(std::move(file)), parameters_(std::move(parameters)) {}

private:
    std::unique_ptr<sem::VariableAccess> file_;
    std::vector<WriteParameter> parameters_;
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
class StatementProcedureWriteln : public Statement {
public:
    StatementProcedureWriteln(
        std::unique_ptr<sem::VariableAccess> &&file,
        std::vector<WriteParameter> &&parameters
    ) : file_(std::move(file)), parameters_(std::move(parameters)) {}

private:
    std::unique_ptr<sem::VariableAccess> file_;
    std::vector<WriteParameter> parameters_;
};

}
