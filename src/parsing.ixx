module;

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module parsing;

import lexing;
import reporting;

using namespace std::literals;

export
class Node;

export
class NodeFieldReceiver {
public:
    virtual
    ~NodeFieldReceiver() = default;

    virtual void
    receiveIdField(std::string_view name, std::string_view value) = 0;

    virtual void
    receiveNodeField(std::string_view name, const Node &value) = 0;

    virtual void
    receiveNodeListField(std::string_view name, std::span<const Node *> value) = 0;
};

export
class Node {
public:
    virtual
    ~Node() = default;

    virtual std::string_view
    type() const = 0;

    virtual void
    describeFields(NodeFieldReceiver &receiver) const {}
};

export
class NodeBlock : public Node {
public:
    std::string_view
    type() const override { return "Block"sv; }
};

export
class NodeProgramParameterDeclaration : public Node {
public:
    std::string name;

    std::string_view
    type() const override { return "ProgramParameterDeclaration"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveIdField("name", name);
    }
};

export
class NodeProgram : public Node {
public:
    std::string name;
    std::vector<NodeProgramParameterDeclaration> parameter_declarations;
    NodeBlock block;

    std::string_view
    type() const override { return "Program"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveIdField("name", name);

        std::vector<const Node *> pd_pointers;
        pd_pointers.reserve(parameter_declarations.size());
        for (const auto &pd: parameter_declarations)
            pd_pointers.push_back(&pd);

        receiver.receiveNodeListField("parameter_declarations", pd_pointers);

        receiver.receiveNodeField("block", block);
    }
};

export
NodeProgram
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
);
