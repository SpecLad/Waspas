// SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
//
// SPDX-License-Identifier: MPL-2.0

module;

#include <cassert>

module lexing;

import :tables;

using namespace std::literals;

std::string
TokenCharacterString::value() const {
    auto v = view();
    std::string result;
    result.reserve(v.size());

    auto it = v.begin();
    ++it; // skip initial apostrophe

    auto it_end = v.end();
    --it_end; // skip final apostrophe

    while (it != it_end) {
        result.push_back(*it);
        if (*it == '\'') it += 2;
        else ++it;
    }

    return result;
}

const std::string TokenIdentifier::HUMAN_REPRESENTATION = "ID"s;

Cisref
TokenIdentifier::spelling() const {
    std::string_view v = view();
    return Cisref(v.data(), v.size());
}

const std::string TokenUnsignedInteger::HUMAN_REPRESENTATION = "INT"s;
const std::string TokenUnsignedReal::HUMAN_REPRESENTATION = "REAL"s;
const std::string TokenCharacterString::HUMAN_REPRESENTATION = "STR"s;
const std::string TokenEof::HUMAN_REPRESENTATION = "EOF"s;

template <typename R>
R
lexOne(
    std::string_view source_fragment,
    const State<R> states[],
    const std::uint8_t transitions[]
) {
    const auto *state = &states[0];
    auto *result_factory = state->result_factory;
    std::size_t token_end_index = 0;

    for (std::size_t i = 0; i < source_fragment.size(); ++i) {
        unsigned char c = std::toupper(source_fragment[i]);
        if (c < state->transition_min_c || c > state->transition_max_c) break;
        const std::uint8_t new_state_index
            = transitions[state->transitions_offset + (c - state->transition_min_c)];
        if (new_state_index == 255) break;

        state = &states[new_state_index];
        if (state->result_factory) {
            result_factory = state->result_factory;
            token_end_index = i + 1;
        }
    }

    if (result_factory)
        return result_factory(source_fragment.substr(0, token_end_index));
    else
        return R{};
}

std::vector<std::unique_ptr<Token>>
lex(std::string_view source, Reporter &reporter) {
    std::vector<std::unique_ptr<Token>> tokens;

    auto it = source.begin();

    bool previous_required_separation = false;

    for (; ;) {
        bool had_separation = false;

        while (
            std::size_t sep_length = lexOne(
                std::string_view(it, source.end()), SEPARATOR_STATES, SEPARATOR_TRANSITIONS)
        ) {
            had_separation = true;
            it += sep_length;
        }

        if (it == source.end()) break;

        std::unique_ptr<Token> token = lexOne(
            std::string_view(it, source.end()), TOKEN_STATES, TOKEN_TRANSITIONS);

        if (token) {
            bool current_requires_separation = token->requiresSeparation();

            if (previous_required_separation && current_requires_separation
                    && !had_separation) {
                reporter.err(&*it, ec::MISSING_SEPARATOR,
                    "a token of type {} must not directly follow {}",
                    token->humanRepresentation(), tokens.back()->humanRepresentation());
            }

            it += token->view().size();
            tokens.push_back(std::move(token));

            previous_required_separation = current_requires_separation;
        }
        else {
            if (std::isprint(*it))
                reporter.err(&*it, ec::INVALID_TOKEN, "invalid token: {}", *it);
            else
                reporter.err(&*it, ec::INVALID_TOKEN,
                    "invalid token with character code {:#x}", (unsigned char)*it);

            ++it;

            previous_required_separation = false;
        }
    }

    tokens.push_back(std::make_unique<TokenEof>(std::string_view(source.end(), source.end())));
    return tokens;
}
