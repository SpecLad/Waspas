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
    class TokenWs ## name : public TokenWordSymbol<TokenWs ## name> { \
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
class TokenIdentifier : public TokenPatternBased<TokenIdentifier> {
public:
    using TokenPatternBased::TokenPatternBased;
    static const std::regex PATTERN;
};

export
class TokenUnsignedInteger : public TokenPatternBased<TokenUnsignedInteger> {
public:
    using TokenPatternBased::TokenPatternBased;
    static const std::regex PATTERN;
};

export
class TokenUnsignedReal : public TokenPatternBased<TokenUnsignedReal> {
public:
    using TokenPatternBased::TokenPatternBased;
    static const std::regex PATTERN;
};

export
class TokenCharacterString : public TokenPatternBased<TokenCharacterString> {
public:
    using TokenPatternBased::TokenPatternBased;
    static const std::regex PATTERN;
};

export
std::vector<std::unique_ptr<Token>>
lex(std::string_view source);
