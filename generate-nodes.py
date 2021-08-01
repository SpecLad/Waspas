#!/usr/bin/env python3

import graphlib

from collections.abc import Collection
from dataclasses import dataclass, field
from typing import Union

@dataclass
class Field:
    name: str

    def get_dependencies(self, node_types):
        return ()

@dataclass
class IdentifierField(Field):
    def generate_declaration(self, node_types):
        print(f'    std::string {self.name};')

    def generate_describe_call(self, node_types):
        print(f'    receiver.receiveIdField("{self.name}", {self.name});')

@dataclass
class BooleanField(Field):
    def generate_declaration(self, node_types):
        print(f'    bool {self.name};')

    def generate_describe_call(self, node_types):
        print(f'    receiver.receiveBooleanField("{self.name}", {self.name});')

@dataclass
class IntegerField(Field):
    def generate_declaration(self, node_types):
        print(f'    pascal_integer_t {self.name};')

    def generate_describe_call(self, node_types):
        print(f'    receiver.receiveIntField("{self.name}", {self.name});')

@dataclass
class RealField(Field):
    def generate_declaration(self, node_types):
        print(f'    pascal_real_t {self.name};')

    def generate_describe_call(self, node_types):
        print(f'    receiver.receiveRealField("{self.name}", {self.name});')

@dataclass
class EnumField(Field):
    enum_type: str

    def generate_declaration(self, node_types):
        print(f'    {self.enum_type} {self.name};')

    def generate_describe_call(self, node_types):
        print(f'    receiver.receiveIdField("{self.name}", asString({self.name}));')

@dataclass
class StringField(Field):
    def generate_declaration(self, node_types):
        print(f'    std::string {self.name};')

    def generate_describe_call(self, node_types):
        print(f'    receiver.receiveStringField("{self.name}", {self.name});')

@dataclass
class NodeField(Field):
    node_type: str

    def generate_declaration(self, node_types):
        if node_types[self.node_type].abstract:
            print(f'    std::unique_ptr<{self.node_type}> {self.name};')
        else:
            print(f'    {self.node_type} {self.name};')

    def generate_describe_call(self, node_types):
        if node_types[self.node_type].abstract:
            print(f'    receiver.receiveNodeField("{self.name}", *{self.name});')
        else:
            print(f'    receiver.receiveNodeField("{self.name}", {self.name});')

    def get_dependencies(self, node_types):
        if node_types[self.node_type].abstract:
            return ()
        else:
            return (self.node_type,)

@dataclass
class NodeListField(Field):
    component_node_type: str

    def generate_declaration(self, node_types):
        if node_types[self.component_node_type].abstract:
            print(f'    std::vector<std::unique_ptr<{self.component_node_type}>> {self.name};')
        else:
            print(f'    std::vector<{self.component_node_type}> {self.name};')

    def generate_describe_call(self, node_types):
        print(f'    describeNodeListField(receiver, "{self.name}", {self.name});')

@dataclass
class OptionalNodeField(Field):
    component_node_type: str

    def generate_declaration(self, node_types):
        assert not node_types[self.component_node_type].abstract
        print(f'    std::optional<{self.component_node_type}> {self.name};')

    def generate_describe_call(self, node_types):
        print(f'    describeOptionalNodeField(receiver, "{self.name}", {self.name});')

    def get_dependencies(self, node_types):
        return (self.component_node_type,)


@dataclass
class NodeType:
    name: str
    bases: Collection[str] = field(default_factory=tuple)
    fields: Collection[Field] = field(default_factory=tuple)
    atomic: bool = False
    abstract: bool = False

    def __post_init__(self):
        assert all(
            self.bases[i] < self.bases[i + 1]
            for i in range(len(self.bases) - 1)
        )

    def get_dependencies(self, node_types):
        return tuple(self.bases) + tuple(dep
            for field in self.fields
            for dep in field.get_dependencies(node_types)
        )

    def generate_forward_declaration(self):
        print('export')
        print(f'class {self.name};')

    def generate_definition(self, node_types):
        print('export')

        if len(self.bases) == 0:
            bases_str = 'public virtual Node'
        else:
            bases_str = ', '.join(f'public {base}' for base in self.bases)

        final = '' if self.abstract else ' final'

        print(f'class {self.name}{final} : {bases_str} {{')
        print('public:')

        for field in self.fields:
            field.generate_declaration(node_types)

        if not self.abstract:
            print('')
            print('    std::string_view')
            print(f'    type() const override {{ return "{self.name}"sv; }}')

        if self.atomic:
            print('')
            print('    bool')
            print('    isAtomic() const override { return true; }')

        print('')
        print('    void')
        print('    describeFields(NodeFieldReceiver &receiver) const override;')
        print('};')

    def generate_describe_fields(self, node_types):
        print('void')
        print(f'{self.name}::describeFields(NodeFieldReceiver &receiver) const {{')

        for base in self.bases:
            print(f'    {base}::describeFields(receiver);')

        for field in self.fields:
            field.generate_describe_call(node_types)

        print('}')


def generate(node_types):
    assert all(
        node_types[i].name < node_types[i + 1].name
        for i in range(len(node_types) - 1)
    )

    print('module;')
    print('#include <memory>')
    print('#include <optional>')
    print('#include <string_view>')
    print('#include <vector>')
    print('export module parsing:nodes;')
    print('export import :core;')
    print('using namespace std::literals;')
    print()

    print('namespace nodes {')

    for node_type in node_types:
        print()
        node_type.generate_forward_declaration()

    node_type_map = {t.name: t for t in node_types}

    sorted_iterator = graphlib.TopologicalSorter({
        node_type.name: node_type.get_dependencies(node_type_map)
        for node_type in node_types
    }).static_order()

    for node_type_name in sorted_iterator:
        print()
        node_type_map[node_type_name].generate_definition(node_type_map)

    for node_type in node_types:
        print()
        node_type.generate_describe_fields(node_type_map)

    print()
    print('}')


NODE_TYPES = (
    NodeType('ActualParameter', fields=(
        NodeField('value', 'Expression'),
        OptionalNodeField('total_width', 'Expression'),
        OptionalNodeField('frac_digits', 'Expression'),
    )),

    NodeType('ArrayType', bases=('UnpackedStructuredType',), fields=(
        NodeListField('index_types', 'OrdinalType'),
        NodeField('component_type', 'TypeDenoter'),
    )),

    NodeType('AssignmentStatement', bases=('UnlabeledStatement',), fields=(
        NodeField('access', 'VariableAccess'),
        NodeField('expression', 'Expression'),
    )),

    NodeType('Block', fields=(
        NodeListField('label_declarations', 'Label'),
        NodeListField('constant_definitions', 'ConstantDefinition'),
        NodeListField('type_definitions', 'TypeDefinition'),
        NodeListField('variable_declarations', 'VariableDeclaration'),
        NodeListField('subroutine_declarations', 'SubroutineDeclaration'),
        NodeField('statement', 'CompoundStatement'),
    )),

    NodeType('CaseListElement', fields=(
        NodeListField('constants', 'Constant'),
        NodeField('statement', 'Statement'),
    )),

    NodeType('CaseStatement', bases=('UnlabeledStatement',), fields=(
        NodeField('case_index', 'Expression'),
        NodeListField('cases', 'CaseListElement'),
    )),

    NodeType('CharacterString', atomic=True, bases=(
        'Constant',
        'Factor',
    ), fields=(
        StringField('value'),
    )),

    NodeType('CompoundStatement', bases=('UnlabeledStatement',), fields=(
        NodeListField('statements', 'Statement'),
    )),

    NodeType('Constant', abstract=True),

    NodeType('ConstantDefinition', fields=(
        NodeField('name', 'Identifier'),
        NodeField('value', 'Constant'),
    )),

    NodeType('DereferencingModifier', bases=('VariableModifier',)),

    NodeType('EmptyStatement', bases=('UnlabeledStatement',)),

    NodeType('EnumeratedType', bases=('OrdinalType',), fields=(
        NodeListField('constants', 'Identifier'),
    )),

    NodeType('Expression', fields=(
        NodeField('factor', 'Factor'), # TODO: replace with proper fields
    )),

    NodeType('Factor', abstract=True),

    NodeType('FieldAccessModifier', bases=('VariableModifier',), fields=(
        NodeField('field', 'Identifier'),
    )),

    NodeType('FieldList', fields=(
        NodeListField('fixed_sections', 'RecordSection'),
        OptionalNodeField('variant_part', 'VariantPart'),
    )),

    NodeType('FileType', bases=('UnpackedStructuredType',), fields=(
        NodeField('component_type', 'TypeDenoter'),
    )),

    NodeType('FormalParameterSection', abstract=True),

    NodeType('FormalParameterTypeOrSchema', abstract=True),

    NodeType('FunctionHeading', bases=(
        'FormalParameterSection',
        'SubroutineHeading',
    ), fields=(
        NodeListField('parameters', 'FormalParameterSection'),
        NodeField('result_type', 'Identifier'),
    )),

    NodeType('FunctionIdentification', bases=('SubroutineHeading',)),

    NodeType('GotoStatement', bases=('UnlabeledStatement',), fields=(
        NodeField('label', 'Label'),
    )),

    NodeType('Identifier', atomic=True, bases=(
        'FormalParameterTypeOrSchema',
        'OrdinalType',
        'SignableConstant',
    ), fields=(
        IdentifierField('spelling'),
    )),

    NodeType('IfStatement', bases=('UnlabeledStatement',), fields=(
        NodeField('condition', 'Expression'),
        NodeField('true_branch', 'Statement'),
        OptionalNodeField('false_branch', 'Statement'),
    )),

    NodeType('IndexTypeSpecification', fields=(
        NodeField('smallest', 'Identifier'),
        NodeField('largest', 'Identifier'),
        NodeField('bound_type', 'Identifier'),
    )),

    NodeType('IndexingModifier', bases=('VariableModifier',), fields=(
        NodeListField('indices', 'Expression'),
    )),

    NodeType('Label', atomic=True, fields=(
        IntegerField('value'),
    )),

    NodeType('NewPointerType', bases=('TypeDenoter',), fields=(
        NodeField('domain_type', 'Identifier'),
    )),

    NodeType('NewStructuredType', bases=('TypeDenoter',), fields=(
        BooleanField('is_packed'),
        NodeField('unpacked', 'UnpackedStructuredType'),
    )),

    NodeType('Nil', bases=('Factor',)),

    NodeType('NotExpression', bases=('Factor',), fields=(
        NodeField('operand', 'Factor'),
    )),

    NodeType('OrdinalType', abstract=True, bases=('TypeDenoter',)),

    NodeType('PackedConformantArraySchema', bases=(
        'FormalParameterTypeOrSchema',
    ), fields=(
        NodeField('index_type', 'IndexTypeSpecification'),
        NodeField('component_type', 'Identifier'),
    )),

    NodeType('Parenthetical', bases=('Factor',), fields=(
        NodeField('inner_expression', 'Expression'),
    )),

    NodeType('ProcedureHeading', bases=(
        'FormalParameterSection',
        'SubroutineHeading',
    ), fields=(
        NodeListField('parameters', 'FormalParameterSection'),
    )),

    # There is no ProcedureIdentification, because it would be indistinguishable
    # from a ProcedureHeading with no parameters.

    NodeType('ProcedureStatement', bases=('UnlabeledStatement',), fields=(
        NodeField('procedure', 'Identifier'),
        NodeListField('parameters', 'ActualParameter'),
    )),

    NodeType('Program', fields=(
        NodeField('name', 'Identifier'),
        NodeListField('parameter_declarations', 'Identifier'),
        NodeField('block', 'Block'),
    )),

    NodeType('RecordSection', fields=(
        NodeListField('field_names', 'Identifier'),
        NodeField('field_type', 'TypeDenoter'),
    )),

    NodeType('RecordType', bases=('UnpackedStructuredType',), fields=(
        NodeField('fields', 'FieldList'),
    )),

    NodeType('RegularParameterSection', bases=(
        'FormalParameterSection',
    ), fields=(
        BooleanField('is_variable'),
        NodeListField('parameter_names', 'Identifier'),
        NodeField('parameter_type', 'FormalParameterTypeOrSchema'),
    )),

    NodeType('SetType', bases=('UnpackedStructuredType',), fields=(
        NodeField('base_type', 'OrdinalType'),
    )),

    NodeType('SignableConstant', abstract=True, bases=('Constant',)),

    NodeType('SignedConstant', bases=('Constant',), fields=(
        EnumField('sign', 'PascalSign'),
        NodeField('unsigned_value', 'SignableConstant'),
    )),

    NodeType('Statement', fields=(
        OptionalNodeField('label', 'Label'),
        NodeField('unlabeled', 'UnlabeledStatement'),
    )),

    NodeType('SubrangeType', bases=('OrdinalType',), fields=(
        NodeField('smallest', 'Constant'),
        NodeField('largest', 'Constant'),
    )),

    NodeType('SubroutineDeclaration', fields=(
        NodeField('heading', 'SubroutineHeading'),
        OptionalNodeField('block', 'Block'),
    )),

    NodeType('SubroutineHeading', abstract=True, fields=(
        NodeField('name', 'Identifier'),
    )),

    NodeType('TypeDefinition', fields=(
        NodeField('name', 'Identifier'),
        NodeField('denoter', 'TypeDenoter'),
    )),

    NodeType('TypeDenoter', abstract=True),

    NodeType('UnlabeledStatement', abstract=True),

    NodeType('UnpackedConformantArraySchema', bases=(
        'FormalParameterTypeOrSchema',
    ), fields=(
        NodeListField('index_types', 'IndexTypeSpecification'),
        NodeField('component_type', 'FormalParameterTypeOrSchema'),
    )),

    NodeType('UnpackedStructuredType', abstract=True),

    NodeType('UnsignedIntegerConstant', atomic=True, bases=(
        'Factor',
        'SignableConstant',
    ), fields=(
        IntegerField('value'),
    )),

    NodeType('UnsignedRealConstant', atomic=True, bases=(
        'Factor',
        'SignableConstant',
    ), fields=(
        RealField('value'),
    )),

    NodeType('VariableAccess', bases=('Factor',), fields=(
        NodeField('variable', 'Identifier'),
        NodeListField('modifiers', 'VariableModifier'),
    )),

    NodeType('VariableDeclaration', fields=(
        NodeListField('var_names', 'Identifier'),
        NodeField('var_type', 'TypeDenoter'),
    )),

    NodeType('VariableModifier', abstract=True),

    NodeType('Variant', fields=(
        NodeListField('case_constants', 'Constant'),
        NodeField('fields', 'FieldList'),
    )),

    NodeType('VariantPart', fields=(
        OptionalNodeField('tag_field', 'Identifier'),
        NodeField('tag_type', 'Identifier'),
        NodeListField('variants', 'Variant'),
    )),
)

def main():
    generate(NODE_TYPES)

if __name__ == '__main__':
    main()
