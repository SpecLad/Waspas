module;

#include <cassert>
#include <memory>
#include <regex>
#include <string_view>
#include <vector>

module lexer;

// TODO: add comments
const std::regex RE_WHITESPACE(R"(^[\t\n\v\f\r ]*)");

template <typename It>
It
skip_whitespace(It begin, It end) {
    std::match_results<It> match;
    bool found = std::regex_search(begin, end, match, RE_WHITESPACE);

    // the regex allows zero-length matches, so it should never fail
    assert(found);

    return begin + match.length();
}

std::vector<std::unique_ptr<Token>>
lex(std::string_view source) {
    std::vector<std::unique_ptr<Token>> tokens;

    auto it = source.begin();
    int32 line = 0, column = 0;

    for (; ;) {
        auto after_ws = skip_whitespace(it, source.end());

        for (; it < after_ws; ++it) {
            if (*it == '\n') {
                ++line;
                column = 0;
            }
            else {
                ++column;
            }
        }

        if (it == source.end()) return tokens;

        tokens.push_back(std::make_unique<TokenPlus>(Locus(line, column), 1));
        ++it;
        ++column;
    }
}
