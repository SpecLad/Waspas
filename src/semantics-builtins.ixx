module;

#include <memory>
#include <vector>

export module semantics:builtins;

export import :expressions;
export import :statements;

namespace sem {

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

}
