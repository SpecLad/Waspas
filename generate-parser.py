#!/usr/bin/env python3

# SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
#
# SPDX-License-Identifier: MPL-2.0

from collections.abc import Callable
import dataclasses

# TODO: remove duplication with generate-lexing-tables.py.
SPECIAL_SYMBOL_NAMES = {
    "-": "Minus",
    ",": "Comma",
    ";": "Semicolon",
    ":": "Colon",
    ":=": "Assign",
    "..": "DotDot",
    ".": "Dot",
    "(": "LeftParenthesis",
    ")": "RightParenthesis",
    "[": "LeftBracket",
    "]": "RightBracket",
    "*": "Asterisk",
    "/": "Slash",
    "^": "Caret",
    "+": "Plus",
    "<": "LessThan",
    "<=": "LessThanOrEqual",
    "<>": "NotEqual",
    "=": "Equal",
    ">": "GreaterThan",
    ">=": "GreaterThanOrEqual",
}

type Expression = Alt | Cat | Opt | Rep | NT | T

@dataclasses.dataclass
class Alt:
    operands: list[Expression]

@dataclasses.dataclass
class Cat:
    operands: list[Expression]

@dataclasses.dataclass
class Opt:
    operand: Expression

@dataclasses.dataclass
class Rep:
    operand: Expression

@dataclasses.dataclass
class NT:
    name: str

@dataclasses.dataclass
class T:
    value: str

GRAMMAR: dict[str, Expression] = {
    "actual-parameter": Alt([
        NT("expression"),
        NT("variable-access"),
        NT("procedure-identifier"),
        NT("function-identifier"),
    ]),
    "actual-parameter-list": Cat(
        [T("("), NT("actual-parameter"), Rep(Cat([T(","), NT("actual-parameter")])), T(")")]
    ),
    "adding-operator": Alt([T("+"), T("-"), T("or")]),
    "array-type": Cat([
        T("array"),
        T("["),
        NT("index-type"),
        Rep(Cat([T(","), NT("index-type")])),
        T("]"),
        T("of"),
        NT("component-type"),
    ]),
    "array-variable": NT("variable-access"),
    "assignment-statement": Cat(
        [Alt([NT("variable-access"), NT("function-identifier")]), T(":="), NT("expression")]
    ),
    "base-type": NT("ordinal-type"),
    "block": Cat([
        NT("label-declaration-part"),
        NT("constant-definition-part"),
        NT("type-definition-part"),
        NT("variable-declaration-part"),
        NT("procedure-and-function-declaration-part"),
        NT("statement-part"),
    ]),
    "Boolean-expression": NT("expression"),
    "bound-identifier": NT("identifier"),
    "buffer-variable": Cat([NT("file-variable"), T("^")]),
    "case-constant": NT("constant"),
    "case-constant-list": Cat([NT("case-constant"), Rep(Cat([T(","), NT("case-constant")]))]),
    "case-index": NT("expression"),
    "case-list-element": Cat([NT("case-constant-list"), T(":"), NT("statement")]),
    "case-statement": Cat([
        T("case"),
        NT("case-index"),
        T("of"),
        NT("case-list-element"),
        Rep(Cat([T(";"), NT("case-list-element")])),
        Opt(T(";")),
        T("end"),
    ]),
    "component-type": NT("type-denoter"),
    "component-variable": Alt([NT("indexed-variable"), NT("field-designator")]),
    "compound-statement": Cat([T("begin"), NT("statement-sequence"), T("end")]),
    "conditional-statement": Alt([NT("if-statement"), NT("case-statement")]),
    "conformant-array-parameter-specification": Alt(
        [NT("value-conformant-array-specification"), NT("variable-conformant-array-specification")]
    ),
    "conformant-array-schema": Alt(
        [NT("packed-conformant-array-schema"), NT("unpacked-conformant-array-schema")]
    ),
    "constant": Alt([
        Cat([Opt(NT("sign")), Alt([NT("unsigned-number"), NT("constant-identifier")])]),
        NT("character-string"),
    ]),
    "constant-definition": Cat([NT("identifier"), T("="), NT("constant")]),
    "constant-definition-part": Opt(
        Cat([
            T("const"),
            NT("constant-definition"),
            T(";"),
            Rep(Cat([NT("constant-definition"), T(";")])),
        ])
    ),
    "constant-identifier": NT("identifier"),
    "control-variable": NT("entire-variable"),
    "domain-type": NT("type-identifier"),
    "else-part": Cat([T("else"), NT("statement")]),
    "empty-statement": Cat([]),
    "entire-variable": NT("variable-identifier"),
    "enumerated-type": Cat([T("("), NT("identifier-list"), T(")")]),
    "expression": Cat(
        [NT("simple-expression"), Opt(Cat([NT("relational-operator"), NT("simple-expression")]))]
    ),
    "factor": Alt([
        NT("bound-identifier"),
        NT("variable-access"),
        NT("unsigned-constant"),
        NT("function-designator"),
        NT("set-constructor"),
        Cat([T("("), NT("expression"), T(")")]),
        Cat([T("not"), NT("factor")]),
    ]),
    "field-designator": Alt([
        Cat([NT("record-variable"), T("."), NT("field-specifier")]),
        NT("field-designator-identifier"),
    ]),
    "field-designator-identifier": NT("identifier"),
    "field-identifier": NT("identifier"),
    "field-list": Opt(
        Cat([
            Alt([
                Cat([NT("fixed-part"), Opt(Cat([T(";"), NT("variant-part")]))]),
                NT("variant-part"),
            ]),
            Opt(T(";")),
        ])
    ),
    "field-specifier": NT("field-identifier"),
    "file-type": Cat([T("file"), T("of"), NT("component-type")]),
    "file-variable": NT("variable-access"),
    "final-value": NT("expression"),
    "fixed-part": Cat([NT("record-section"), Rep(Cat([T(";"), NT("record-section")]))]),
    "for-statement": Cat([
        T("for"),
        NT("control-variable"),
        T(":="),
        NT("initial-value"),
        Alt([T("to"), T("downto")]),
        NT("final-value"),
        T("do"),
        NT("statement"),
    ]),
    "formal-parameter-list": Cat([
        T("("),
        NT("formal-parameter-section"),
        Rep(Cat([T(";"), NT("formal-parameter-section")])),
        T(")"),
    ]),
    "formal-parameter-section": Alt([
        NT("value-parameter-specification"),
        NT("variable-parameter-specification"),
        NT("procedural-parameter-specification"),
        NT("functional-parameter-specification"),
        NT("conformant-array-parameter-specification"),
    ]),
    "function-block": NT("block"),
    "function-declaration": Alt([
        Cat([NT("function-heading"), T(";"), NT("directive")]),
        Cat([NT("function-identification"), T(";"), NT("function-block")]),
        Cat([NT("function-heading"), T(";"), NT("function-block")]),
    ]),
    "function-designator": Cat([NT("function-identifier"), Opt(NT("actual-parameter-list"))]),
    "function-heading": Cat([
        T("function"),
        NT("identifier"),
        Opt(NT("formal-parameter-list")),
        T(":"),
        NT("result-type"),
    ]),
    "function-identification": Cat([T("function"), NT("function-identifier")]),
    "function-identifier": NT("identifier"),
    "functional-parameter-specification": NT("function-heading"),
    "goto-statement": Cat([T("goto"), NT("label")]),
    "identified-variable": Cat([NT("pointer-variable"), T("^")]),
    "identifier-list": Cat([NT("identifier"), Rep(Cat([T(","), NT("identifier")]))]),
    "if-statement": Cat(
        [T("if"), NT("Boolean-expression"), T("then"), NT("statement"), Opt(NT("else-part"))]
    ),
    "index-expression": NT("expression"),
    "index-type": NT("ordinal-type"),
    "index-type-specification": Cat(
        [NT("identifier"), T(".."), NT("identifier"), T(":"), NT("ordinal-type-identifier")]
    ),
    "indexed-variable": Cat([
        NT("array-variable"),
        T("["),
        NT("index-expression"),
        Rep(Cat([T(","), NT("index-expression")])),
        T("]"),
    ]),
    "initial-value": NT("expression"),
    "label-declaration-part": Opt(
        Cat([T("label"), NT("label"), Rep(Cat([T(","), NT("label")])), T(";")])
    ),
    "member-designator": Cat([NT("expression"), Opt(Cat([T(".."), NT("expression")]))]),
    "multiplying-operator": Alt([T("*"), T("/"), T("div"), T("mod"), T("and")]),
    "new-ordinal-type": Alt([NT("enumerated-type"), NT("subrange-type")]),
    "new-pointer-type": Cat([T("^"), NT("domain-type")]),
    "new-structured-type": Cat([Opt(T("packed")), NT("unpacked-structured-type")]),
    "new-type": Alt([NT("new-ordinal-type"), NT("new-structured-type"), NT("new-pointer-type")]),
    "ordinal-type": Alt([NT("new-ordinal-type"), NT("ordinal-type-identifier")]),
    "ordinal-type-identifier": NT("type-identifier"),
    "packed-conformant-array-schema": Cat([
        T("packed"),
        T("array"),
        T("["),
        NT("index-type-specification"),
        T("]"),
        T("of"),
        NT("type-identifier"),
    ]),
    "pointer-type-identifier": NT("type-identifier"),
    "pointer-variable": NT("variable-access"),
    "procedural-parameter-specification": NT("procedure-heading"),
    "procedure-and-function-declaration-part": Rep(
        Cat([Alt([NT("procedure-declaration"), NT("function-declaration")]), T(";")])
    ),
    "procedure-block": NT("block"),
    "procedure-declaration": Alt([
        Cat([NT("procedure-heading"), T(";"), NT("directive")]),
        Cat([NT("procedure-identification"), T(";"), NT("procedure-block")]),
        Cat([NT("procedure-heading"), T(";"), NT("procedure-block")]),
    ]),
    "procedure-heading": Cat([T("procedure"), NT("identifier"), Opt(NT("formal-parameter-list"))]),
    "procedure-identification": Cat([T("procedure"), NT("procedure-identifier")]),
    "procedure-identifier": NT("identifier"),
    "procedure-statement": Cat([
        NT("procedure-identifier"),
        Alt([
            Opt(NT("actual-parameter-list")),
            NT("read-parameter-list"),
            NT("readln-parameter-list"),
            NT("write-parameter-list"),
            NT("writeln-parameter-list"),
        ]),
    ]),
    "program": Cat([NT("program-heading"), T(";"), NT("program-block"), T(".")]),
    "program-block": NT("block"),
    "program-heading": Cat(
        [T("program"), NT("identifier"), Opt(Cat([T("("), NT("program-parameter-list"), T(")")]))]
    ),
    "program-parameter-list": NT("identifier-list"),
    "read-parameter-list": Cat([
        T("("),
        Opt(Cat([NT("file-variable"), T(",")])),
        NT("variable-access"),
        Rep(Cat([T(","), NT("variable-access")])),
        T(")"),
    ]),
    "readln-parameter-list": Opt(
        Cat([
            T("("),
            Alt([NT("file-variable"), NT("variable-access")]),
            Rep(Cat([T(","), NT("variable-access")])),
            T(")"),
        ])
    ),
    "record-section": Cat([NT("identifier-list"), T(":"), NT("type-denoter")]),
    "record-type": Cat([T("record"), NT("field-list"), T("end")]),
    "record-variable": NT("variable-access"),
    "record-variable-list": Cat([NT("record-variable"), Rep(Cat([T(","), NT("record-variable")]))]),
    "relational-operator": Alt([T("="), T("<>"), T("<"), T(">"), T("<="), T(">="), T("in")]),
    "repeat-statement": Cat(
        [T("repeat"), NT("statement-sequence"), T("until"), NT("Boolean-expression")]
    ),
    "repetitive-statement": Alt(
        [NT("repeat-statement"), NT("while-statement"), NT("for-statement")]
    ),
    "result-type": Alt([NT("simple-type-identifier"), NT("pointer-type-identifier")]),
    "set-constructor": Cat([
        T("["),
        Opt(Cat([NT("member-designator"), Rep(Cat([T(","), NT("member-designator")]))])),
        T("]"),
    ]),
    "set-type": Cat([T("set"), T("of"), NT("base-type")]),
    "sign": Alt([T("+"), T("-")]),
    "simple-expression": Cat(
        [Opt(NT("sign")), NT("term"), Rep(Cat([NT("adding-operator"), NT("term")]))]
    ),
    "simple-statement": Alt([
        NT("empty-statement"),
        NT("assignment-statement"),
        NT("procedure-statement"),
        NT("goto-statement"),
    ]),
    "simple-type-identifier": NT("type-identifier"),
    "statement": Cat(
        [Opt(Cat([NT("label"), T(":")])), Alt([NT("simple-statement"), NT("structured-statement")])]
    ),
    "statement-part": NT("compound-statement"),
    "statement-sequence": Cat([NT("statement"), Rep(Cat([T(";"), NT("statement")]))]),
    "structured-statement": Alt([
        NT("compound-statement"),
        NT("conditional-statement"),
        NT("repetitive-statement"),
        NT("with-statement"),
    ]),
    "subrange-type": Cat([NT("constant"), T(".."), NT("constant")]),
    "tag-field": NT("identifier"),
    "tag-type": NT("ordinal-type-identifier"),
    "term": Cat([NT("factor"), Rep(Cat([NT("multiplying-operator"), NT("factor")]))]),
    "type-definition": Cat([NT("identifier"), T("="), NT("type-denoter")]),
    "type-definition-part": Opt(
        Cat([T("type"), NT("type-definition"), T(";"), Rep(Cat([NT("type-definition"), T(";")]))])
    ),
    "type-denoter": Alt([NT("type-identifier"), NT("new-type")]),
    "type-identifier": NT("identifier"),
    "unpacked-conformant-array-schema": Cat([
        T("array"),
        T("["),
        NT("index-type-specification"),
        Rep(Cat([T(";"), NT("index-type-specification")])),
        T("]"),
        T("of"),
        Alt([NT("type-identifier"), NT("conformant-array-schema")]),
    ]),
    "unpacked-structured-type": Alt(
        [NT("array-type"), NT("record-type"), NT("set-type"), NT("file-type")]
    ),
    "unsigned-constant": Alt(
        [NT("unsigned-number"), NT("character-string"), NT("constant-identifier"), T("nil")]
    ),
    "unsigned-number": Alt([NT("unsigned-integer"), NT("unsigned-real")]),
    "value-conformant-array-specification": Cat(
        [NT("identifier-list"), T(":"), NT("conformant-array-schema")]
    ),
    "value-parameter-specification": Cat([NT("identifier-list"), T(":"), NT("type-identifier")]),
    "variable-access": Alt([
        NT("entire-variable"),
        NT("component-variable"),
        NT("identified-variable"),
        NT("buffer-variable"),
    ]),
    "variable-conformant-array-specification": Cat(
        [T("var"), NT("identifier-list"), T(":"), NT("conformant-array-schema")]
    ),
    "variable-declaration": Cat([NT("identifier-list"), T(":"), NT("type-denoter")]),
    "variable-declaration-part": Opt(
        Cat([
            T("var"),
            NT("variable-declaration"),
            T(";"),
            Rep(Cat([NT("variable-declaration"), T(";")])),
        ])
    ),
    "variable-identifier": NT("identifier"),
    "variable-parameter-specification": Cat(
        [T("var"), NT("identifier-list"), T(":"), NT("type-identifier")]
    ),
    "variant": Cat([NT("case-constant-list"), T(":"), T("("), NT("field-list"), T(")")]),
    "variant-part": Cat([
        T("case"),
        NT("variant-selector"),
        T("of"),
        NT("variant"),
        Rep(Cat([T(";"), NT("variant")])),
    ]),
    "variant-selector": Cat([Opt(Cat([NT("tag-field"), T(":")])), NT("tag-type")]),
    "while-statement": Cat([T("while"), NT("Boolean-expression"), T("do"), NT("statement")]),
    "with-statement": Cat([T("with"), NT("record-variable-list"), T("do"), NT("statement")]),
    "write-parameter": Cat([
        NT("expression"),
        Opt(Cat([T(":"), NT("expression"), Opt(Cat([T(":"), NT("expression")]))])),
    ]),
    "write-parameter-list": Cat([
        T("("),
        Opt(Cat([NT("file-variable"), T(",")])),
        NT("write-parameter"),
        Rep(Cat([T(","), NT("write-parameter")])),
        T(")"),
    ]),
    "writeln-parameter-list": Opt(
        Cat([
            T("("),
            Alt([NT("file-variable"), NT("write-parameter")]),
            Rep(Cat([T(","), NT("write-parameter")])),
            T(")"),
        ])
    ),
}

STARTING_RULE = "program"

TOKEN_NTS = {"character-string", "identifier", "unsigned-integer", "unsigned-real"}

ALIAS_TOKEN_NTS = {"label": "unsigned-integer", "directive": "identifier"}

def visit(expr: Expression, accept: Callable[[Expression], object]) -> None:
    match expr:
        case Alt(ops) | Cat(ops):
            for op in ops:
                visit(op, accept)
        case Opt(op) | Rep(op):
            visit(op, accept)

    accept(expr)

def pascal_case(kebab_case: str) -> str:
    parts = kebab_case.split("-")
    return ''.join(part.title() for part in parts)

def validate() -> None:
    unreferenced_nts = {*GRAMMAR.keys(), *TOKEN_NTS, *ALIAS_TOKEN_NTS.keys()}

    def check_leaves(expr: Expression) -> None:
        match expr:
            case NT(name):
                assert name in GRAMMAR or name in TOKEN_NTS or name in ALIAS_TOKEN_NTS
                unreferenced_nts.discard(name)
            case T(name):
                assert name.isalpha() or name in SPECIAL_SYMBOL_NAMES

    for expr in GRAMMAR.values():
        visit(expr, check_leaves)

    assert unreferenced_nts == {STARTING_RULE}

def print_utility_call(name: str, subparsers: list[tuple[str, Expression]]) -> None:
    print(f"    return parse{name}(")

    for subid, _ in subparsers[:-1]:
        print(f"        &Parser::parse{subid},")
    if subparsers:
        print(f"        &Parser::parse{subparsers[-1][0]}")

    print("    );")

def print_parser(name: str | None, identifier: str, expr: Expression) -> None:
        print("std::unique_ptr<Node>")
        print(f"parse{identifier}() {{")

        subparsers: list[tuple[str, Expression]] = []

        match expr:
            case Alt(operands):
                for i, op in enumerate(operands):
                    subparsers.append((f"{identifier}_alt{i}", op))

                print_utility_call("Alternation", subparsers)

            case Cat(operands):
                for i, op in enumerate(operands):
                    subparsers.append((f"{identifier}_part{i}", op))

                print_utility_call("Concatenation", subparsers)

            case Opt(operand):
                subparsers.append((f"{identifier}_item", operand))

                print_utility_call("Optionality", subparsers)

            case Rep(operand):
                subparsers.append((f"{identifier}_item", operand))

                print_utility_call("Repetition", subparsers)

            case NT(nt_name):
                print(f"    return parse{pascal_case(nt_name)}();")

            case T(value):
                if value.isalpha():
                    token_name = "Ws" + pascal_case(value)
                else:
                    token_name = SPECIAL_SYMBOL_NAMES[value]

                print(f"    return parseSpecialSymbol<Token{token_name}>();")

            case _:
                assert False

        print("}")
        print()

        for subid, subexpr in subparsers:
            print_parser(None, subid, subexpr)

def main() -> None:
    for name, expr in GRAMMAR.items():
        print_parser(name, pascal_case(name), expr)

if __name__ == '__main__':
    main()
