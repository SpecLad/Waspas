module;

#include <memory>
#include <span>
#include <string>
#include <string_view>

export module parsing;

import lexing;
import reporting;

using namespace std::literals;

export
class NodeFieldReceiver {
public:
    virtual
    ~NodeFieldReceiver() = default;

    virtual void
    receiveIdField(std::string_view name, std::string_view value) = 0;
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
class NodeProgram : public Node {
public:
    std::string name;

    std::string_view
    type() const override { return "Program"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveIdField("name", name);
    }
};

export
NodeProgram
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
);
