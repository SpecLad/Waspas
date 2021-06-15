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

export // the export is not needed, but without it, VC++ produces an ICE
template<typename T>
class TokenSpecialSymbol : public Token {
public:
    using Token::Token;

    static std::unique_ptr<T>
    tryLex(std::string_view source_fragment);
};

export // same as for the previous export
template<typename T>
class TokenSpecialSymbolWithAlt : public Token {
public:
    using Token::Token;

    static std::unique_ptr<T>
        tryLex(std::string_view source_fragment);
};

export
class TokenPlus : public TokenSpecialSymbol<TokenPlus> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = "+";
};

export
class TokenMinus : public TokenSpecialSymbol<TokenMinus> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = "-";
};

export
class TokenAsterisk : public TokenSpecialSymbol<TokenAsterisk> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = "*";
};

export
class TokenSlash : public TokenSpecialSymbol<TokenSlash> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = "/";
};

export
class TokenEqual : public TokenSpecialSymbol<TokenEqual> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = "=";
};

export
class TokenLessThan : public TokenSpecialSymbol<TokenLessThan> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = "<";
};

export
class TokenGreaterThan : public TokenSpecialSymbol<TokenGreaterThan> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = ">";
};

export
class TokenLeftBracket : public TokenSpecialSymbolWithAlt<TokenLeftBracket> {
public:
    using TokenSpecialSymbolWithAlt::TokenSpecialSymbolWithAlt;
    static inline constexpr char REPRESENTATION[] = "[";
    static inline constexpr char ALTERNATIVE_REPRESENTATION[] = "(.";
};

export
class TokenRightBracket : public TokenSpecialSymbolWithAlt<TokenRightBracket> {
public:
    using TokenSpecialSymbolWithAlt::TokenSpecialSymbolWithAlt;
    static inline constexpr char REPRESENTATION[] = "]";
    static inline constexpr char ALTERNATIVE_REPRESENTATION[] = ".)";
};

export
class TokenDot : public TokenSpecialSymbol<TokenDot> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = ".";
};

export
class TokenComma : public TokenSpecialSymbol<TokenComma> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = ",";
};

export
class TokenColon : public TokenSpecialSymbol<TokenColon> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = ":";
};

export
class TokenSemicolon : public TokenSpecialSymbol<TokenSemicolon> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = ";";
};

export
class TokenCaret : public TokenSpecialSymbolWithAlt<TokenCaret> {
public:
    using TokenSpecialSymbolWithAlt::TokenSpecialSymbolWithAlt;
    static inline constexpr char REPRESENTATION[] = "^";
    static inline constexpr char ALTERNATIVE_REPRESENTATION[] = "@";
};

export
class TokenLeftParenthesis : public TokenSpecialSymbol<TokenLeftParenthesis> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = "(";
};

export
class TokenRightParenthesis : public TokenSpecialSymbol<TokenRightParenthesis> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = ")";
};

export
class TokenNotEqual : public TokenSpecialSymbol<TokenNotEqual> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = "<>";
};

export
class TokenLessThanOrEqual : public TokenSpecialSymbol<TokenLessThanOrEqual> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = "<=";
};

export
class TokenGreaterThanOrEqual : public TokenSpecialSymbol<TokenGreaterThanOrEqual> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = ">=";
};

export
class TokenAssign : public TokenSpecialSymbol<TokenAssign> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = ":=";
};

export
class TokenDotDot : public TokenSpecialSymbol<TokenDotDot> {
public:
    using TokenSpecialSymbol::TokenSpecialSymbol;
    static inline constexpr char REPRESENTATION[] = "..";
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
