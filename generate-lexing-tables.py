#!/usr/bin/env python3

# SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import argparse
import dataclasses
import functools
import string
import textwrap
from collections.abc import Iterable, Set
from typing import Protocol

type Expression = CharClass | Concat | Either | Maybe | OneOrMore
type Grammar = dict[str, Expression]

@dataclasses.dataclass
class CharClass:
    codes: Set[str]

    @classmethod
    def of(cls, codes: Iterable[str]) -> CharClass:
        return CharClass(frozenset(c.upper() for c in codes))

@dataclasses.dataclass
class Concat:
    left: Expression
    right: Expression

    @classmethod
    def many(cls, first: Expression, *rest: Expression) -> Expression:
        return functools.reduce(cls, rest, first)

@dataclasses.dataclass
class Either:
    top: Expression
    bottom: Expression

@dataclasses.dataclass
class Maybe:
    element: Expression

@dataclasses.dataclass
class OneOrMore:
    element: Expression

def lit(spelling: str) -> Expression:
    char_classes: Iterable[Expression] = map(CharClass.of, spelling)
    return functools.reduce(Concat, char_classes)

LETTER = CharClass.of(string.ascii_letters)
DIGIT = CharClass.of(string.digits)
LETTER_OR_DIGIT = CharClass.of(string.ascii_letters + string.digits)
DIGITS = OneOrMore(DIGIT)
EXPONENT = Concat.many(lit("e"), Maybe(CharClass.of("+-")), DIGITS)
STRING_ELEMENT = Either(
    CharClass.of({*string.ascii_letters, *string.digits, *string.punctuation, " "} - {"'"}),
    lit("''"),
)

TOKENS: Grammar = {
    "Identifier": Concat(LETTER, Maybe(OneOrMore(LETTER_OR_DIGIT))),
    "UnsignedInteger": OneOrMore(DIGIT),
    "UnsignedReal": Concat(
        DIGITS,
        Either(Concat.many(lit("."), DIGITS, Maybe(EXPONENT)), EXPONENT)
    ),
    "CharacterString": Concat.many(lit("'"), OneOrMore(STRING_ELEMENT), lit("'")),
}

PRIORITY_TOKENS: list[str] = []

def add_special_symbol(name: str, *spellings: str) -> None:
    TOKENS[name] = functools.reduce(Either, map(lit, spellings))

add_special_symbol("Plus", "+")
add_special_symbol("Minus", "-")
add_special_symbol("Asterisk", "*")
add_special_symbol("Slash", "/")
add_special_symbol("Equal", "=")
add_special_symbol("LessThan", "<")
add_special_symbol("GreaterThan", ">")
add_special_symbol("LeftBracket", "[", "(.")
add_special_symbol("RightBracket", "]", ".)")
add_special_symbol("Dot", ".")
add_special_symbol("Comma", ",")
add_special_symbol("Colon", ":")
add_special_symbol("Semicolon", ";")
add_special_symbol("Caret", "^", "@")
add_special_symbol("LeftParenthesis", "(")
add_special_symbol("RightParenthesis", ")")
add_special_symbol("NotEqual", "<>")
add_special_symbol("LessThanOrEqual", "<=")
add_special_symbol("GreaterThanOrEqual", ">=")
add_special_symbol("Assign", ":=")
add_special_symbol("DotDot", "..")

def add_word_symbol(spelling: str) -> None:
    token_name = f"Ws{spelling}"
    TOKENS[token_name] = lit(spelling)
    PRIORITY_TOKENS.append(token_name)

add_word_symbol("And")
add_word_symbol("Array")
add_word_symbol("Begin")
add_word_symbol("Case")
add_word_symbol("Const")
add_word_symbol("Div")
add_word_symbol("Do")
add_word_symbol("Downto")
add_word_symbol("Else")
add_word_symbol("End")
add_word_symbol("File")
add_word_symbol("For")
add_word_symbol("Function")
add_word_symbol("Goto")
add_word_symbol("If")
add_word_symbol("In")
add_word_symbol("Label")
add_word_symbol("Mod")
add_word_symbol("Nil")
add_word_symbol("Not")
add_word_symbol("Of")
add_word_symbol("Or")
add_word_symbol("Packed")
add_word_symbol("Procedure")
add_word_symbol("Program")
add_word_symbol("Record")
add_word_symbol("Repeat")
add_word_symbol("Set")
add_word_symbol("Then")
add_word_symbol("To")
add_word_symbol("Type")
add_word_symbol("Until")
add_word_symbol("Var")
add_word_symbol("While")
add_word_symbol("With")

# If we start supporting non-ASCII characters, this will need to be adjusted.
ALL_CHARS = frozenset(map(chr, range(128)))

SEPARATORS: Grammar = {
    "Whitespace": CharClass.of(string.whitespace),
    "Comment": Concat.many(
        Either(lit("{"), lit("(*")),
        Maybe(OneOrMore(Either(
            CharClass.of(ALL_CHARS - {"*", "}"}),
            Concat(OneOrMore(lit("*")), CharClass.of(ALL_CHARS - {"*", ")", "}"})),
        ))),
        Maybe(OneOrMore(lit("*"))),
        Either(lit("}"), lit("*)")),
    ),
}

GRAMMARS = {"Token": TOKENS, "Separator": SEPARATORS}

class FaState(Protocol):
    @property
    def result(self) -> str | None: ...

    def transition_iter(self) -> Iterable[tuple[str, FaState]]: ...

@dataclasses.dataclass
class NfaState:
    result: str | None = None
    transitions: list[tuple[str, NfaState]] = dataclasses.field(default_factory=list)

    def transition_iter(self) -> Iterable[tuple[str, NfaState]]:
        return self.transitions

@dataclasses.dataclass
class DfaState:
    result: str | None = None
    transitions: dict[str, DfaState] = dataclasses.field(default_factory=dict)

    def transition_iter(self) -> Iterable[tuple[str, DfaState]]:
        return self.transitions.items()

def convert_expression_to_enfa(start: NfaState, expression: Expression) -> NfaState:
    match expression:
        case CharClass(chars):
            end = NfaState()
            for c in sorted(chars): # sort to get deterministic ordering
                start.transitions.append((c, end))
            return end

        case Concat(left, right):
            middle = convert_expression_to_enfa(start, left)
            return convert_expression_to_enfa(middle, right)

        case Either(top, bottom):
            top_end = convert_expression_to_enfa(start, top)
            bottom_end = convert_expression_to_enfa(start, bottom)
            end = NfaState()
            top_end.transitions.append(("", end))
            bottom_end.transitions.append(("", end))
            return end

        case Maybe(element):
            end = convert_expression_to_enfa(start, element)
            start.transitions.append(("", end))
            return end

        case OneOrMore(element):
            middle = NfaState()
            start.transitions.append(("", middle))
            end = convert_expression_to_enfa(middle, element)
            end.transitions.append(("", middle))
            return end

def convert_grammar_to_enfa(grammar: Grammar) -> NfaState:
    enfa_start = NfaState()

    for token_name, token_expression in grammar.items():
        token_end = convert_expression_to_enfa(enfa_start, token_expression)
        token_end.result = token_name

    return enfa_start

def eliminate_epsilons(estart: NfaState) -> NfaState:
    # Maps id(state) in the e-NFA to the pair of:
    # * state in the NFA
    # * map of id(state) -> state of the e-NFA states in the epsilon closure
    estate_to_ndesc: dict[int, tuple[NfaState, dict[int, NfaState]]] = {}

    def get_ndesc(estate: NfaState) -> tuple[NfaState, dict[int, NfaState]]:
        if ndesc := estate_to_ndesc.get(id(estate)):
            return ndesc

        closure = {id(estate): estate}
        for c, dest in estate.transitions:
            if not c:
                closure.update(get_ndesc(dest)[1])

        nstate = NfaState()
        ndesc = nstate, closure
        estate_to_ndesc[id(estate)] = ndesc

        for estate in closure.values():
            if estate.result:
                # We could, in principle, handle the case where there are multiple conflicting
                # results, but it doesn't come up in the Pascal grammar, so we don't need to.
                assert nstate.result in {None, estate.result}
                nstate.result = estate.result

            for c, dest in estate.transitions:
                if c:
                    nstate.transitions.append((c, get_ndesc(dest)[0]))

        return ndesc

    return get_ndesc(estart)[0]

def convert_nfa_to_dfa(nstart: NfaState) -> DfaState:
    nset_to_dstate: dict[frozenset[int], DfaState] = {}

    def get_dstate(nstates: Iterable[NfaState]) -> DfaState:
        key = frozenset(id(nstate) for nstate in nstates)
        if dstate := nset_to_dstate.get(key):
            return dstate

        result = None
        for nstate in nstates:
            if nstate.result:
                assert (
                    not result
                    or result == nstate.result
                    or nstate.result in PRIORITY_TOKENS and result not in PRIORITY_TOKENS
                )
                result = nstate.result

        dstate = DfaState(result)

        nset_to_dstate[key] = dstate

        char_to_ndests: dict[str, list[NfaState]] = {}

        for nstate in nstates:
            for c, ndest in nstate.transitions:
                char_to_ndests.setdefault(c, []).append(ndest)

        for c, ndests in char_to_ndests.items():
            dstate.transitions[c] = get_dstate(ndests)

        return dstate

    return get_dstate([nstart])

def minimize_dfa(start: DfaState) -> None:
    id_to_state: dict[int, DfaState] = {}

    def collect_states(state: DfaState) -> None:
        if id(state) in id_to_state: return

        id_to_state[id(state)] = state
        for _, dest in state.transition_iter():
            collect_states(dest)

    collect_states(start)

    marked_pairs: set[tuple[int, int]] = set()
    unmarked_pairs: set[tuple[int, int]] = set()

    for i1, n1 in id_to_state.items():
        for i2, n2 in id_to_state.items():
            if n1 is not n2:
                (unmarked_pairs if n1.result == n2.result else marked_pairs).add((i1, i2))

    while True:
        changes_made = False

        for pair in [*unmarked_pairs]:
            i1, i2 = pair
            n1 = id_to_state[i1]
            n2 = id_to_state[i2]

            for c in n1.transitions.keys() | n2.transitions.keys():
                dest1 = n1.transitions.get(c)
                dest2 = n2.transitions.get(c)
                if not dest1 or not dest2 or (id(dest1), id(dest2)) in marked_pairs:
                    unmarked_pairs.remove(pair)
                    marked_pairs.add(pair)
                    changes_made = True
                    break

        if not changes_made:
            break

    id_to_canonical_id: dict[int, int] = {}

    for i1, i2 in unmarked_pairs:
        # This is probably not true in the general case, but it doesn't happen here,
        # and it's simpler not to handle it.
        assert id(start) not in (i1, i2)

        if i2 < i1:
            cid = id_to_canonical_id.get(i1)
            if cid is None or i2 < cid:
                id_to_canonical_id[i1] = i2

    for state in id_to_state.values():
        if id(state) in id_to_canonical_id:
            continue

        for c, dest in [*state.transition_iter()]:
            cid = id_to_canonical_id.get(id(dest))
            if cid is not None:
                state.transitions[c] = id_to_state[cid]

def todotstr(s: str) -> str:
    return '"' + s.replace('\\', '\\\\').replace('"', r'\"') + '"'

def rangify(chars: Set[str]) -> str:
    ranges = []
    if '' in chars:
        ranges.append("\N{GREEK LUNATE EPSILON SYMBOL}")

    range_start = None
    prev = None

    def add_range() -> None:
        nonlocal range_start
        if range_start:
            if range_start == prev:
                ranges.append(repr(range_start))
            else:
                ranges.append(repr(range_start) + '..' + repr(prev))
            range_start = None

    for n in range(0, 0x7f):
        cur = chr(n)
        if cur in chars:
            if not range_start:
                range_start = cur
        else:
            add_range()
        prev = cur

    add_range()

    return ", ".join(ranges)

def dump_grammars() -> None:
    print("digraph {")

    next_node_id = 0

    def dump_expression(node_id: int, expression: Expression) -> None:
        nonlocal next_node_id

        match expression:
            case CharClass(codes):
                print(f"    {node_id} [label={todotstr(rangify(codes))}]")
            case _:
                print(f"    {node_id} [label={todotstr(expression.__class__.__name__)}]")

                for field in dataclasses.fields(expression):
                    child = getattr(expression, field.name)

                    child_node_id = next_node_id
                    next_node_id += 1

                    print(f"    {node_id} -> {child_node_id} [label={todotstr(field.name)}]")
                    dump_expression(child_node_id, child)

    for grammar in GRAMMARS.values():
        for token_name, expression in grammar.items():
            print(f'    {token_name} [shape="rectangle"]')
            node_id = next_node_id
            next_node_id += 1
            print(f"    {token_name} -> {node_id}")
            dump_expression(node_id, expression)

    print("}")

def dump_fas(starts: Iterable[FaState]) -> None:
    print("digraph {")
    print("    rankdir=LR;")

    state_id_to_node_id: dict[int, int] = {}

    next_node_id = 0

    def dump_state(state: FaState) -> int:
        nonlocal next_node_id

        node_id = state_id_to_node_id.get(id(state))
        if node_id is not None:
            return node_id

        node_id = next_node_id
        next_node_id += 1

        state_id_to_node_id[id(state)] = node_id

        print(f'    {node_id} [label={todotstr(state.result or "")}]')

        chars_per_dest: dict[int, tuple[set[str], FaState]] = {}

        for c, dest in state.transition_iter():
            chars_per_dest.setdefault(id(dest), (set(), dest))[0].add(c)

        for chars, dest in chars_per_dest.values():
            dest_node_id = dump_state(dest)
            print(f'    {node_id} -> {dest_node_id} [label={todotstr(rangify(chars))}]')

        return node_id

    for start in starts:
        dump_state(start)

    print("}")

def generate_tables(name: str, start: DfaState) -> None:
    state_id_to_index: dict[int, int] = {}
    states_by_index: list[DfaState] = []

    DEAD_END = 255

    def assign_state_index(state: DfaState) -> None:
        if id(state) in state_id_to_index: return

        next_index = len(states_by_index)
        assert next_index < DEAD_END
        state_id_to_index[id(state)] = next_index
        states_by_index.append(state)

        for dest in state.transitions.values():
            assign_state_index(dest)

    assign_state_index(start)

    print()
    print(f"constexpr State<Grammar{name}::result_type> {name.upper()}_STATES[] = {{")

    def c_char(c: str) -> str:
        if c == "'": return r"'\''"

        n = ord(c)
        if 0x20 <= n < 0x7f:
            return f"'{c}'"
        return rf"'\x{n:02x}'"

    next_transition_offset = 0
    transitions_to_offset: dict[tuple[int, ...], int] = {}

    for state in states_by_index:
        min_c_repr: object
        max_c_repr: object

        if state.transitions:
            min_c = min(c for c in state.transitions.keys())
            min_c_repr = c_char(min_c)
            max_c = max(c for c in state.transitions.keys())
            max_c_repr = c_char(max_c)
            num_transitions = ord(max_c) - ord(min_c) + 1

            transitions_list = [DEAD_END] * num_transitions

            for c, dest in state.transition_iter():
                transitions_list[ord(c) - ord(min_c)] = state_id_to_index[id(dest)]

            transitions = tuple(transitions_list)

            transition_offset = transitions_to_offset.get(transitions)
            if transition_offset is None:
                transitions_to_offset[transitions] = transition_offset = next_transition_offset
                next_transition_offset += num_transitions
        else:
            min_c_repr = 1
            max_c_repr = 0
            transition_offset = 0

        print("    {{{:3}, {:3}, {:4}, {}}},".format(
            min_c_repr, max_c_repr, transition_offset,
            f"&Grammar{name}::makeResult<{name}{state.result}>" if state.result else 'nullptr',
        ))

    print("};")
    print()

    print(f"constexpr std::uint8_t {name.upper()}_TRANSITIONS[] = {{")

    for transitions in transitions_to_offset:
        print("    ", end='')
        for transition in transitions:
            print(f"{transition}, ", end='')
        print()

    print("};")

def main() -> None:
    parser = argparse.ArgumentParser()
    stage_args = parser.add_mutually_exclusive_group()
    stage_args.add_argument('--grammars', action='store_true', help="dump the grammars")
    stage_args.add_argument('--enfas', action='store_true', help="dump the e-NFAs")
    stage_args.add_argument('--nfas', action='store_true', help="dump the NFAs")
    stage_args.add_argument('--dfas', action='store_true', help="dump the DFAs")
    stage_args.add_argument('--min-dfas', action='store_true', help="dump the minimized DFAs")
    args = parser.parse_args()

    if args.grammars:
        dump_grammars()
        return

    enfa_starts = {name: convert_grammar_to_enfa(grammar) for name, grammar in GRAMMARS.items()}

    if args.enfas:
        dump_fas(enfa_starts.values())
        return

    nfa_starts = {name: eliminate_epsilons(enfa_start) for name, enfa_start in enfa_starts.items()}

    if args.nfas:
        dump_fas(nfa_starts.values())
        return

    dfa_starts = {name: convert_nfa_to_dfa(nfa_start) for name, nfa_start in nfa_starts.items()}

    if args.dfas:
        dump_fas(dfa_starts.values())
        return

    for dfa_start in dfa_starts.values():
        minimize_dfa(dfa_start)

    if args.min_dfas:
        dump_fas(dfa_starts.values())
        return

    print(textwrap.dedent("""\
        module lexing:tables;
        import :tokens;
        import std;

        template <typename R>
        struct State {
            unsigned char transition_min_c, transition_max_c;
            std::uint16_t transitions_offset;
            R (*result_factory)(std::string_view view);
        };
    """), end='')

    for name, dfa_start in dfa_starts.items():
        generate_tables(name, dfa_start)

if __name__ == '__main__':
    main()
