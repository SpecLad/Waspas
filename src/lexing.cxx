module;

#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

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

std::string
TokenIdentifier::spelling() const {
    std::string s(view());
    for (auto &&c: s)
        c = std::tolower(c);
    return s;
}

const std::string TokenUnsignedInteger::HUMAN_REPRESENTATION = "INT"s;
const std::string TokenUnsignedReal::HUMAN_REPRESENTATION = "REAL"s;
const std::string TokenCharacterString::HUMAN_REPRESENTATION = "STR"s;
const std::string TokenEof::HUMAN_REPRESENTATION = "EOF"s;

template <typename It>
It
skipSeparators(It begin, It end) {
    // Visual Studio's std::regex implementation overflows the stack when
    // trying to match a long comment. See
    // <https://developercommunity.visualstudio.com/t/grouping-within-repetition-causes-regex-stack-erro/885115>.
    // So here's a hand-rolled state machine instead.

    enum class State {
        NEUTRAL, IN_COMMENT, MAYBE_COMMENT_START, MAYBE_COMMENT_END,
    };

    State current_state = State::NEUTRAL;
    It comment_start;

    for (It it = begin; it < end; ++it) {
        switch (current_state) {
        case State::NEUTRAL:
            switch (*it) {
            case '\t':
            case '\n':
            case '\v':
            case '\f':
            case '\r':
            case ' ':
                break; // whitespace - no state change
            case '{':
                current_state = State::IN_COMMENT;
                comment_start = it;
                break;
            case '(':
                current_state = State::MAYBE_COMMENT_START;
                comment_start = it;
                break;
            default:
                return it;
            }
            break;

        case State::IN_COMMENT:
            switch (*it) {
            case '}':
                current_state = State::NEUTRAL;
                break;
            case '*':
                current_state = State::MAYBE_COMMENT_END;
                break;
            default:
                break; // comment continues - no state change
            }
            break;

        case State::MAYBE_COMMENT_START:
            switch (*it) {
            case '*':
                current_state = State::IN_COMMENT;
                break;
            default:
                return comment_start; // wasn't a comment after all
            }
            break;

        case State::MAYBE_COMMENT_END:
            switch (*it) {
            case ')':
            case '}':
                current_state = State::NEUTRAL;
                break;
            case '*':
                break; // new possible *) sequence - no state change
            default:
                current_state = State::IN_COMMENT;
                break;
            }
            break;
        }
    }

    if (current_state == State::NEUTRAL)
        return end;
    else
        return comment_start; // unclosed comment
}

std::unique_ptr<Token>
lexOne(std::string_view source_fragment) {
    const State *state = &TOKEN_STATES[0];
    auto *result_factory = state->result_factory;
    std::size_t token_end_index = 0;

    for (std::size_t i = 0; i < source_fragment.size(); ++i) {
        if (state->result_factory) {
            result_factory = state->result_factory;
            token_end_index = i;
        }

        unsigned char c = std::toupper(source_fragment[i]);
        if (c < state->transition_min_c || c > state->transition_max_c) break;
        const std::uint8_t new_state_index
            = TOKEN_TRANSITIONS[state->transitions_offset + (c - state->transition_min_c)];
        if (new_state_index == 255) break;
        state = &TOKEN_STATES[new_state_index];
    }

    if (result_factory)
        return result_factory(source_fragment.substr(0, token_end_index));
    else
        return nullptr;
}

std::vector<std::unique_ptr<Token>>
lex(std::string_view source, Reporter &reporter) {
    std::vector<std::unique_ptr<Token>> tokens;

    auto it = source.begin();

    bool previous_required_separation = false;

    for (; ;) {
        auto after_separators = skipSeparators(it, source.end());
        bool had_separation = after_separators != it;
        it = after_separators;

        if (it == source.end()) break;

        std::unique_ptr<Token> token = lexOne(std::string_view(it, source.end()));

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
