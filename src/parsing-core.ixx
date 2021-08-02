module;

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

export module parsing:core;

using namespace std::literals;

export
using pascal_integer_t = std::int32_t;

export
using pascal_real_t = double;

static_assert(sizeof(pascal_real_t) == 8);
static_assert(std::numeric_limits<pascal_real_t>::is_iec559);
static_assert(std::numeric_limits<pascal_real_t>::digits == 53);

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
    receiveBooleanField(std::string_view name, bool value) = 0;

    virtual void
    receiveIntField(std::string_view name, pascal_integer_t value) = 0;

    virtual void
    receiveRealField(std::string_view name, pascal_real_t value) = 0;

    virtual void
    receiveStringField(std::string_view name, std::string_view value) = 0;

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

    virtual bool
    isAtomic() const { return false; }

    virtual void
    describeFields(NodeFieldReceiver &receiver) const {}

    std::string_view view;

protected:
    template <typename T>
    static void
    describeNodeListField(
        NodeFieldReceiver &receiver,
        std::string_view name,
        const std::vector<T> &nodes
    ) {
        std::vector<const Node *> pointers;
        pointers.reserve(nodes.size());

        for (const auto &node: nodes)
            if constexpr (std::is_base_of_v<Node, T>)
                pointers.push_back(&node);
            else
                pointers.push_back(std::to_address(node));

        receiver.receiveNodeListField(name, pointers);
    }

    template <typename T>
    static void
    describeOptionalNodeField(
        NodeFieldReceiver &receiver,
        std::string_view name,
        const std::optional<T> &maybe_node
    ) {
        if (maybe_node) {
            const Node *pointers[] = { &*maybe_node };
            receiver.receiveNodeListField(name, pointers);
        }
        else {
            receiver.receiveNodeListField(name, {});
        }
    }
};
