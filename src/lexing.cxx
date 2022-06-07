module;

#include <cassert>
#include <cctype>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

module lexing;

using namespace std::literals;

template <typename T>
std::unique_ptr<T>
TokenSpecialSymbol<T>::tryLex(std::string_view source_fragment) {
    if (source_fragment.starts_with(T::REPRESENTATION)) {
        return std::make_unique<T>(
            std::string_view(source_fragment.data(), sizeof T::REPRESENTATION - 1));
    }

    return nullptr;
}

template <typename T>
std::unique_ptr<T>
TokenSpecialSymbolWithAlt<T>::tryLex(std::string_view source_fragment) {
    if (source_fragment.starts_with(T::REPRESENTATION)) {
        return std::make_unique<T>(
            std::string_view(source_fragment.data(), sizeof T::REPRESENTATION - 1));
    }

    if (source_fragment.starts_with(T::ALTERNATIVE_REPRESENTATION)) {
        return std::make_unique<T>(
            std::string_view(source_fragment.data(), sizeof T::ALTERNATIVE_REPRESENTATION - 1));
    }

    return nullptr;
}

template <typename T>
std::unique_ptr<T>
TokenPatternBased<T>::tryLex(std::string_view source_fragment) {
    std::match_results<std::string_view::iterator> match;

    if (!std::regex_search(source_fragment.begin(), source_fragment.end(),
            match, T::PATTERN, std::regex_constants::match_continuous))
        return nullptr;

    return std::make_unique<T>(
        std::string_view(source_fragment.data(), match.length()));
}

const std::regex TokenIdentifier::PATTERN(R"([a-z][a-z0-9]*)",
    std::regex_constants::ECMAScript | std::regex_constants::icase);

const std::regex TokenUnsignedInteger::PATTERN(R"([0-9]+)",
    std::regex_constants::ECMAScript | std::regex_constants::icase);

const std::regex TokenUnsignedReal::PATTERN(R"([0-9]+(?:\.[0-9]+(?:e[+-]?[0-9]+)?|e[+-]?[0-9]+))",
    std::regex_constants::ECMAScript | std::regex_constants::icase);

const std::regex TokenCharacterString::PATTERN(R"('(?:[^'\n]|'')+')");

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

// Fake token class that produces either TokenIdentifier or TokenWordSymbol
// instances.
class TokenIdentifierOrWs {
public:
    static std::unique_ptr<Token>
    tryLex(std::string_view source_fragment) {
        // Since the regex for TokenIdentifier also matches word symbols,
        // we can save time by only matching the identifier regex with
        // the input stream and then checking if the spelling corresponds
        // to one of the word symbols (instead of trying to lex each word
        // symbol via its own regex).
        auto token_identifier = TokenIdentifier::tryLex(source_fragment);
        if (!token_identifier) return nullptr;

        auto it = WS_FACTORIES.find(token_identifier->spelling());
        if (it == WS_FACTORIES.end()) return token_identifier;

        return it->second(token_identifier->view());
    }

private:
    using token_factory_f = std::unique_ptr<Token>(*)(std::string_view view);
    using token_factory_map_t = std::unordered_map<std::string, token_factory_f>;

    static const token_factory_map_t WS_FACTORIES;

    template <typename T>
    static std::unique_ptr<Token>
    makeToken(std::string_view view) {
        return std::make_unique<T>(view);
    }

    static std::string
    lowercase(std::string_view s) {
        std::string result(s);
        for (auto &c : result) c = std::tolower(c);
        return result;
    }

    template<typename ...Ts>
    static token_factory_map_t
    makeTokenFactories() {
        return token_factory_map_t{
            {lowercase(Ts::REPRESENTATION), &makeToken<Ts>}...};
    }
};

const TokenIdentifierOrWs::token_factory_map_t
TokenIdentifierOrWs::WS_FACTORIES = makeTokenFactories<
    TokenWsAnd,
    TokenWsArray,
    TokenWsBegin,
    TokenWsCase,
    TokenWsConst,
    TokenWsDiv,
    TokenWsDo,
    TokenWsDownto,
    TokenWsElse,
    TokenWsEnd,
    TokenWsFile,
    TokenWsFor,
    TokenWsFunction,
    TokenWsGoto,
    TokenWsIf,
    TokenWsIn,
    TokenWsLabel,
    TokenWsMod,
    TokenWsNil,
    TokenWsNot,
    TokenWsOf,
    TokenWsOr,
    TokenWsPacked,
    TokenWsProcedure,
    TokenWsProgram,
    TokenWsRecord,
    TokenWsRepeat,
    TokenWsSet,
    TokenWsThen,
    TokenWsTo,
    TokenWsType,
    TokenWsUntil,
    TokenWsVar,
    TokenWsWhile,
    TokenWsWith
>();

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

template <typename T0, typename ...Ts>
std::unique_ptr<Token>
lexOne(std::string_view source_fragment) {
    if (auto token = T0::tryLex(source_fragment))
        return token;

    if constexpr (sizeof...(Ts) > 0)
        return lexOne<Ts...>(source_fragment);
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

        std::unique_ptr<Token> token = lexOne<
            // word symbols, identifiers and literals
            TokenIdentifierOrWs,
            // TokenUnsignedReal must precede TokenUnsignedInteger,
            // or TokenUnsignedInteger will eat the integer part of real literals
            TokenUnsignedReal,
            TokenUnsignedInteger,
            TokenCharacterString,

            // special symbol tokens

            // the two-character symbols have to go first in order to not get preempted
            // by the tokens corresponding to their initial characters
            TokenNotEqual,
            TokenLessThanOrEqual,
            TokenGreaterThanOrEqual,
            TokenAssign,
            TokenDotDot,

            // these tokens have two-character alternative representations,
            // so they have to be prioritized for the same reason
            TokenLeftBracket,
            TokenRightBracket,

            // rest of the special symbol tokens
            TokenPlus,
            TokenMinus,
            TokenAsterisk,
            TokenSlash,
            TokenEqual,
            TokenLessThan,
            TokenGreaterThan,
            TokenDot,
            TokenComma,
            TokenColon,
            TokenSemicolon,
            TokenCaret,
            TokenLeftParenthesis,
            TokenRightParenthesis
        >(std::string_view(it, source.end()));

        if (token) {
            bool current_requires_separation = token->requiresSeparation();

            if (previous_required_separation && current_requires_separation
                    && !had_separation) {
                reporter.err(&*it, "missing-separator",
                    "a token of type {} must not directly follow {}",
                    token->humanRepresentation(), tokens.back()->humanRepresentation());
            }

            it += token->view().size();
            tokens.push_back(std::move(token));

            previous_required_separation = current_requires_separation;
        }
        else {
            if (std::isprint(*it))
                reporter.err(&*it, "invalid-token", "invalid token: {}", *it);
            else
                reporter.err(&*it, "invalid-token",
                    "invalid token with character code {:#x}", (unsigned char)*it);

            ++it;

            previous_required_separation = false;
        }
    }

    tokens.push_back(std::make_unique<TokenEof>(std::string_view(source.end(), source.end())));
    return tokens;
}
