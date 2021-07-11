module;

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

export module parsing;

import lexing;
import reporting;

using namespace std::literals;

export
using pascal_integer_t = std::int32_t;

export
using pascal_real_t = double;

static_assert(sizeof(pascal_real_t) == 8);
static_assert(std::numeric_limits<pascal_real_t>::is_iec559);
static_assert(std::numeric_limits<pascal_real_t>::digits == 53);

export
enum class PascalSign {
    PLUS, MINUS,
};

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
            if constexpr (std::is_base_of_v<Node, T>)
                pointers.push_back(&node);
            else
                pointers.push_back(std::to_address(node));

        receiver.receiveNodeListField(name, pointers);
    }

    template <typename T>
    static void
    declareOptionalNodeField(
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

namespace nodes {

export
class LabelDeclaration : public Node {
public:
    int value;

    std::string_view
    type() const override { return "LabelDeclaration"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveIntField("value", value);
    }
};

export
class Constant : public virtual Node {};

export
class UnsignedConstant : public Constant {};

export
class TypeDenoter : public virtual Node {};

export
class OrdinalType : public TypeDenoter {};

export
class UnpackedStructuredType : public virtual Node {};

export
class UnsignedIntegerConstant : public UnsignedConstant {
public:
    pascal_integer_t value;

    std::string_view
    type() const override { return "UnsignedIntegerConstant"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveIntField("value", value);
    }
};

export
class UnsignedRealConstant : public UnsignedConstant {
public:
    pascal_real_t value;

    std::string_view
    type() const override { return "UnsignedRealConstant"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveRealField("value", value);
    }
};

export
class Identifier : public UnsignedConstant, public OrdinalType {
public:
    std::string name;

    std::string_view
    type() const override { return "Identifier"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveIdField("name", name);
    }
};

export
class SignedConstant : public Constant {
public:
    PascalSign sign;
    std::unique_ptr<Constant> unsigned_value;

    std::string_view
    type() const override { return "SignedConstant"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveIdField("sign", sign == PascalSign::PLUS ? "PLUS" : "MINUS");
        receiver.receiveNodeField("unsigned_value", *unsigned_value);
    }
};

export
class CharacterString : public Constant {
public:
    std::string value;

    std::string_view
    type() const override { return "CharacterString"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveStringField("value", value);
    }
};

export
class ConstantDefinition : public Node {
public:
    std::string name;
    std::unique_ptr<Constant> value;

    std::string_view
    type() const override { return "ConstantDefinition"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveIdField("name", name);
        receiver.receiveNodeField("value", *value);
    }
};

export
class EnumeratedType : public OrdinalType {
public:
    std::vector<Identifier> constants;

    std::string_view
    type() const override { return "EnumeratedType"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        declareNodeListField(receiver, "constants", constants);
    }
};

export
class SubrangeType : public OrdinalType {
public:
    std::unique_ptr<Constant> smallest, largest;

    std::string_view
    type() const override { return "SubrangeType"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveNodeField("smallest", *smallest);
        receiver.receiveNodeField("largest", *largest);
    }
};

export
class ArrayType : public UnpackedStructuredType {
public:
    std::vector<std::unique_ptr<OrdinalType>> index_types;
    std::unique_ptr<TypeDenoter> component_type;

    std::string_view
    type() const override { return "ArrayType"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        declareNodeListField(receiver, "index_types", index_types);
        receiver.receiveNodeField("component_type", *component_type);
    }
};

export
class RecordSection : public Node {
public:
    std::vector<Identifier> field_names;
    std::unique_ptr<TypeDenoter> field_type;

    std::string_view
    type() const override { return "RecordSection"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        declareNodeListField(receiver, "field_names", field_names);
        receiver.receiveNodeField("field_type", *field_type);
    }
};

export
class Variant;

export
class VariantPart : public Node {
public:
    std::optional<Identifier> tag_field;
    Identifier tag_type;
    std::vector<Variant> variants;

    std::string_view
    type() const override { return "VariantPart"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        declareOptionalNodeField(receiver, "tag_field", tag_field);
        receiver.receiveNodeField("tag_type", tag_type);
        declareNodeListField(receiver, "variants", variants);
    }
};

export
class FieldList : public Node {
public:
    std::vector<RecordSection> fixed_sections;
    std::optional<VariantPart> variant_part;

    std::string_view
    type() const override { return "FieldList"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        declareNodeListField(receiver, "fixed_sections", fixed_sections);
        declareOptionalNodeField(receiver, "variant_part", variant_part);
    }
};

export
class Variant : public Node {
public:
    std::vector<std::unique_ptr<Constant>> case_constants;
    FieldList fields;

    std::string_view
    type() const override { return "Variant"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        declareNodeListField(receiver, "case_constants", case_constants);
        receiver.receiveNodeField("fields", fields);
    }
};

export
class RecordType : public UnpackedStructuredType {
public:
    FieldList fields;

    std::string_view
    type() const override { return "RecordType"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveNodeField("fields", fields);
    }
};

export
class SetType : public UnpackedStructuredType {
public:
    std::unique_ptr<OrdinalType> base_type;

    std::string_view
    type() const override { return "SetType"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveNodeField("base_type", *base_type);
    }
};

export
class FileType : public UnpackedStructuredType {
public:
    std::unique_ptr<TypeDenoter> component_type;

    std::string_view
    type() const override { return "FileType"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveNodeField("component_type", *component_type);
    }
};

export
class NewStructuredType : public TypeDenoter {
public:
    bool is_packed;
    std::unique_ptr<UnpackedStructuredType> unpacked;

    std::string_view
    type() const override { return "NewStructuredType"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveBooleanField("is_packed", is_packed);
        receiver.receiveNodeField("unpacked", *unpacked);
    }
};

export
class NewPointerType : public TypeDenoter {
public:
    Identifier domain_type;

    std::string_view
    type() const override { return "NewPointerType"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveNodeField("domain_type", domain_type);
    }
};

export
class TypeDefinition : public Node {
public:
    std::string name;
    std::unique_ptr<TypeDenoter> denoter;

    std::string_view
    type() const override { return "TypeDefinition"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        receiver.receiveIdField("name", name);
        receiver.receiveNodeField("denoter", *denoter);
    }
};

export
class Block : public Node {
public:
    std::vector<LabelDeclaration> label_declarations;
    std::vector<ConstantDefinition> constant_definitions;
    std::vector<TypeDefinition> type_definitions;

    std::string_view
    type() const override { return "Block"sv; }

    void
    describeFields(NodeFieldReceiver &receiver) const override {
        declareNodeListField(receiver, "label_declarations", label_declarations);
        declareNodeListField(receiver, "constant_definitions", constant_definitions);
        declareNodeListField(receiver, "type_definitions", type_definitions);
    }
};

export
class Program : public Node {
public:
    std::string name;
    std::vector<Identifier> parameter_declarations;
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
