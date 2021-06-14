module;

#include <cassert>
#include <memory>
#include <regex>
#include <string_view>
#include <vector>

export module lexer;

export
class Token {
public:
    Token(std::string_view view) : view_(view)
    {
        assert(!view.empty());
    }

    std::string_view
    view() const { return view_; }

    virtual ~Token() = default;

private:
    std::string_view view_;
};

export
class TokenPlus : public Token {
public:
    using Token::Token;

    static inline const char REPRESENTATION[] = "+";
};

export
class TokenIdentifier : public Token {
public:
    using Token::Token;

    static const std::regex PATTERN;

    static std::unique_ptr<TokenIdentifier>
    tryLex(std::string_view source_fragment);
};

export
class TokenUnsignedInteger : public Token {
public:
    using Token::Token;
};

export
class TokenUnsignedReal : public Token {
public:
    using Token::Token;
};

export
class TokenCharacterString : public Token {
public:
    using Token::Token;
};

export
std::vector<std::unique_ptr<Token>>
lex(std::string_view source);
