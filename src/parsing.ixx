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
    receiveIntField(std::string_view name, int value) = 0;

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

    std::string_view view;

protected:
    template <typename T>
    static void
    declareNodeListField(
        NodeFieldReceiver &receiver,
        std::string_view name,
        const std::vector<T> &nodes
    ) {
        std::vector<const Node *> pointers;
        pointers.reserve(nodes.size());

        for (const auto &node: nodes)
            pointers.push_back(&node);

        receiver.receiveNodeListField(name, pointers);
    }
};

namespace nodes {

export
class LabelDeclaration : public Node {
public:
    int value;

    std::string_view
    type() const override { return "LabelDeclaration"sv; }

    virtual void
    describeFields(NodeFieldReceiver &receiver) const {
        receiver.receiveIntField("value", value);
    }
};

export
class Block : public Node {
public:
    std::vector<LabelDeclaration> label_declarations;

    std::string_view
    type() const override { return "Block"sv; }

    virtual void
    describeFields(NodeFieldReceiver &receiver) const {
        declareNodeListField(receiver, "label_declarations", label_declarations);
    }
};

export
class ProgramParameterDeclaration : public Node {
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
class Program : public Node {
public:
    std::string name;
    std::vector<ProgramParameterDeclaration> parameter_declarations;
    Block block;

    std::string_view
    type() const override { return "Program"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveIdField("name", name);
        declareNodeListField(receiver, "parameter_declarations", parameter_declarations);
        receiver.receiveNodeField("block", block);
    }
};

}

export
nodes::Program
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
);
