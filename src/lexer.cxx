module;

#include <cassert>
#include <memory>
#include <regex>
#include <string_view>
#include <vector>

module lexer;

template <typename T>
std::unique_ptr<T>
TokenSpecialSymbol<T>::tryLex(std::string_view source_fragment) {
    if (source_fragment.starts_with(T::REPRESENTATION)) {
        return std::make_unique<T>(
            std::string_view(source_fragment.data(), sizeof T::REPRESENTATION - 1));
    }

    return nullptr;
}


const std::regex TokenIdentifier::PATTERN(R"([a-z][a-z0-9]*)",
    std::regex_constants::ECMAScript | std::regex_constants::icase);

std::unique_ptr<TokenIdentifier>
TokenIdentifier::tryLex(std::string_view source_fragment) {
    std::match_results<std::string_view::iterator> match;

    if (!std::regex_search(source_fragment.begin(), source_fragment.end(),
            match, PATTERN, std::regex_constants::match_continuous))
        return nullptr;

    return std::make_unique<TokenIdentifier>(
        std::string_view(source_fragment.data(), match.length()));
}

// TODO: add comments
const std::regex RE_WHITESPACE(R"([\t\n\v\f\r ]*)");

template <typename It>
It
skip_whitespace(It begin, It end) {
    std::match_results<It> match;
    bool found = std::regex_search(begin, end, match, RE_WHITESPACE,
        std::regex_constants::match_continuous);

    // the regex allows zero-length matches, so it should never fail
    assert(found);

    return begin + match.length();
}

template <typename T0, typename ...Ts>
std::unique_ptr<Token>
lexOne(std::string_view source_fragment) {
    if (auto token = T0::tryLex(source_fragment))
        return token;

    if constexpr (sizeof...(Ts) > 0) {
        return lexOne<Ts...>(source_fragment);
    }
    else {
        // dummy fallback; TODO: remove this later
        return std::make_unique<Token>(
            std::string_view(source_fragment.data(), source_fragment.data() + 1));
    }
}

std::vector<std::unique_ptr<Token>>
lex(std::string_view source) {
    std::vector<std::unique_ptr<Token>> tokens;

    auto it = source.begin();

    for (; ;) {
        it = skip_whitespace(it, source.end());

        if (it == source.end()) return tokens;

        std::unique_ptr<Token> token = lexOne<
            TokenIdentifier,

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
            // TODO: actually implement alternative representations
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

        it += token->view().size();

        tokens.push_back(std::move(token));
    }
}
