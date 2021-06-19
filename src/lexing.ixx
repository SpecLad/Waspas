module;

#include <cassert>
#include <memory>
#include <regex>
#include <string_view>
#include <vector>

export module lexing;

import reporting;

export
class Token {
public:
    Token(std::string_view view) : view_(view)
    {}

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

#define DECLARE_SPECIAL_SYMBOL(name, representation) \
    export \
    class Token ## name final : public TokenSpecialSymbol<Token ## name> { \
    public: \
        using TokenSpecialSymbol::TokenSpecialSymbol; \
        static inline constexpr char REPRESENTATION[] = representation; \
    };

#define DECLARE_SPECIAL_SYMBOL_WITH_ALT(name, representation, alt_representation) \
    export \
    class Token ## name final: public TokenSpecialSymbolWithAlt<Token ## name> { \
    public: \
        using TokenSpecialSymbolWithAlt::TokenSpecialSymbolWithAlt; \
        static inline constexpr char REPRESENTATION[] = representation; \
        static inline constexpr char ALTERNATIVE_REPRESENTATION[] = alt_representation; \
    };

DECLARE_SPECIAL_SYMBOL(Plus, "+")
DECLARE_SPECIAL_SYMBOL(Minus, "-")
DECLARE_SPECIAL_SYMBOL(Asterisk, "*")
DECLARE_SPECIAL_SYMBOL(Slash, "/")
DECLARE_SPECIAL_SYMBOL(Equal, "=")
DECLARE_SPECIAL_SYMBOL(LessThan, "<")
DECLARE_SPECIAL_SYMBOL(GreaterThan, ">")
DECLARE_SPECIAL_SYMBOL_WITH_ALT(LeftBracket, "[", "(.")
DECLARE_SPECIAL_SYMBOL_WITH_ALT(RightBracket, "]", ".)")
DECLARE_SPECIAL_SYMBOL(Dot, ".")
DECLARE_SPECIAL_SYMBOL(Comma, ",")
DECLARE_SPECIAL_SYMBOL(Colon, ":")
DECLARE_SPECIAL_SYMBOL(Semicolon, ";")
DECLARE_SPECIAL_SYMBOL_WITH_ALT(Caret, "^", "@")
DECLARE_SPECIAL_SYMBOL(LeftParenthesis, "(")
DECLARE_SPECIAL_SYMBOL(RightParenthesis, ")")
DECLARE_SPECIAL_SYMBOL(NotEqual, "<>")
DECLARE_SPECIAL_SYMBOL(LessThanOrEqual, "<=")
DECLARE_SPECIAL_SYMBOL(GreaterThanOrEqual, ">=")
DECLARE_SPECIAL_SYMBOL(Assign, ":=")
DECLARE_SPECIAL_SYMBOL(DotDot, "..")

export
template <typename T>
class TokenPatternBased : public Token {
public:
    using Token::Token;

    static std::unique_ptr<T>
    tryLex(std::string_view source_fragment);
};

export
template<typename T>
class TokenWordSymbol : public TokenPatternBased<T> {
public:
    using TokenPatternBased<T>::TokenPatternBased;
    static const std::regex PATTERN;
};

#define DECLARE_WORD_SYMBOL(name) \
    export \
    class TokenWs ## name final : public TokenWordSymbol<TokenWs ## name> { \
    public: \
        using TokenWordSymbol::TokenWordSymbol; \
        static inline constexpr char REPRESENTATION[] = #name; \
    };

DECLARE_WORD_SYMBOL(And)
DECLARE_WORD_SYMBOL(Array)
DECLARE_WORD_SYMBOL(Begin)
DECLARE_WORD_SYMBOL(Case)
DECLARE_WORD_SYMBOL(Const)
DECLARE_WORD_SYMBOL(Div)
DECLARE_WORD_SYMBOL(Do)
DECLARE_WORD_SYMBOL(Downto)
DECLARE_WORD_SYMBOL(Else)
DECLARE_WORD_SYMBOL(End)
DECLARE_WORD_SYMBOL(File)
DECLARE_WORD_SYMBOL(For)
DECLARE_WORD_SYMBOL(Function)
DECLARE_WORD_SYMBOL(Goto)
DECLARE_WORD_SYMBOL(If)
DECLARE_WORD_SYMBOL(In)
DECLARE_WORD_SYMBOL(Label)
DECLARE_WORD_SYMBOL(Mod)
DECLARE_WORD_SYMBOL(Nil)
DECLARE_WORD_SYMBOL(Not)
DECLARE_WORD_SYMBOL(Of)
DECLARE_WORD_SYMBOL(Or)
DECLARE_WORD_SYMBOL(Packed)
DECLARE_WORD_SYMBOL(Procedure)
DECLARE_WORD_SYMBOL(Program)
DECLARE_WORD_SYMBOL(Record)
DECLARE_WORD_SYMBOL(Repeat)
DECLARE_WORD_SYMBOL(Set)
DECLARE_WORD_SYMBOL(Then)
DECLARE_WORD_SYMBOL(To)
DECLARE_WORD_SYMBOL(Type)
DECLARE_WORD_SYMBOL(Until)
DECLARE_WORD_SYMBOL(Var)
DECLARE_WORD_SYMBOL(While)
DECLARE_WORD_SYMBOL(With)

export
class TokenIdentifier final : public TokenPatternBased<TokenIdentifier> {
public:
    using TokenPatternBased::TokenPatternBased;
    static const std::regex PATTERN;
};

export
class TokenUnsignedInteger final : public TokenPatternBased<TokenUnsignedInteger> {
public:
    using TokenPatternBased::TokenPatternBased;
    static const std::regex PATTERN;
};

export
class TokenUnsignedReal final : public TokenPatternBased<TokenUnsignedReal> {
public:
    using TokenPatternBased::TokenPatternBased;
    static const std::regex PATTERN;
};

export
class TokenCharacterString final : public TokenPatternBased<TokenCharacterString> {
public:
    using TokenPatternBased::TokenPatternBased;
    static const std::regex PATTERN;
};

export
class TokenEof final : public Token {
public:
    using Token::Token;
};

export
std::vector<std::unique_ptr<Token>>
lex(std::string_view source, Reporter &reporter);
