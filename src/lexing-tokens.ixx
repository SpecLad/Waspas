// SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
//
// SPDX-License-Identifier: MPL-2.0

module;

#include <cassert>
#include <cctype>
#include <charconv>
#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

export module lexing:tokens;

import utilities;

export
class Token {
public:
    Token(std::string_view view) : view_(view)
    {}

    virtual
    ~Token() = default;

    std::string_view
    view() const { return view_; }

    virtual std::string_view
    humanRepresentation() const = 0;

    virtual bool
    requiresSeparation() { return false; }

private:
    std::string_view view_;
};

template <typename T>
class TokenWithQuotedHR : public Token {
public:
    using Token::Token;

    static const std::string HUMAN_REPRESENTATION;
    std::string_view
    humanRepresentation() const override { return HUMAN_REPRESENTATION; }
};

template <std::size_t N>
std::string
quoteStringLiteral(const char (&s)[N]) {
    constexpr auto literal_len = N - 1; // discount the trailing NUL

    std::string result;
    result.reserve(literal_len + 2);
    result.push_back('"');
    result.append(s, literal_len);
    result.push_back('"');

    for (auto &&c: result) c = std::tolower(c);

    return result;
}

template <typename T>
const std::string TokenWithQuotedHR<T>::HUMAN_REPRESENTATION
    = quoteStringLiteral(T::REPRESENTATION);

template<typename T>
class TokenSpecialSymbol : public TokenWithQuotedHR<T> {
public:
    using TokenWithQuotedHR<T>::TokenWithQuotedHR;
};

template<typename T>
class TokenSpecialSymbolWithAlt : public TokenWithQuotedHR<T> {
public:
    using TokenWithQuotedHR<T>::TokenWithQuotedHR;
};

#define DECLARE_SPECIAL_SYMBOL(name, representation) \
    export \
    class Token ## name final : public TokenSpecialSymbol<Token ## name> { \
    public: \
        using TokenSpecialSymbol::TokenSpecialSymbol; \
        static inline constexpr char REPRESENTATION[] = representation; \
    };

DECLARE_SPECIAL_SYMBOL(Plus, "+")
DECLARE_SPECIAL_SYMBOL(Minus, "-")
DECLARE_SPECIAL_SYMBOL(Asterisk, "*")
DECLARE_SPECIAL_SYMBOL(Slash, "/")
DECLARE_SPECIAL_SYMBOL(Equal, "=")
DECLARE_SPECIAL_SYMBOL(LessThan, "<")
DECLARE_SPECIAL_SYMBOL(GreaterThan, ">")
DECLARE_SPECIAL_SYMBOL(LeftBracket, "[")
DECLARE_SPECIAL_SYMBOL(RightBracket, "]")
DECLARE_SPECIAL_SYMBOL(Dot, ".")
DECLARE_SPECIAL_SYMBOL(Comma, ",")
DECLARE_SPECIAL_SYMBOL(Colon, ":")
DECLARE_SPECIAL_SYMBOL(Semicolon, ";")
DECLARE_SPECIAL_SYMBOL(Caret, "^")
DECLARE_SPECIAL_SYMBOL(LeftParenthesis, "(")
DECLARE_SPECIAL_SYMBOL(RightParenthesis, ")")
DECLARE_SPECIAL_SYMBOL(NotEqual, "<>")
DECLARE_SPECIAL_SYMBOL(LessThanOrEqual, "<=")
DECLARE_SPECIAL_SYMBOL(GreaterThanOrEqual, ">=")
DECLARE_SPECIAL_SYMBOL(Assign, ":=")
DECLARE_SPECIAL_SYMBOL(DotDot, "..")

template<typename T>
class TokenWordSymbol : public TokenWithQuotedHR<T> {
public:
    using TokenWithQuotedHR<T>::TokenWithQuotedHR;

    bool
    requiresSeparation() override { return true; }
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

template <typename T>
class TokenWithCustomHR : public Token {
    using Token::Token;

    std::string_view
    humanRepresentation() const override { return T::HUMAN_REPRESENTATION; }
};

export
class TokenIdentifier final
    : public TokenWithCustomHR<TokenIdentifier>
{
public:
    using TokenWithCustomHR::TokenWithCustomHR;
    static const std::string HUMAN_REPRESENTATION;

    bool
    requiresSeparation() override { return true; }

    Cisref
    spelling() const;
};

export
class TokenUnsignedInteger final
    : public TokenWithCustomHR<TokenUnsignedInteger>
{
public:
    using TokenWithCustomHR::TokenWithCustomHR;
    static const std::string HUMAN_REPRESENTATION;

    bool
    requiresSeparation() override { return true; }

    template <std::integral T>
    std::optional<T>
    value() const {
        auto v = view();
        T value;
        auto conversion_result
            = std::from_chars(v.data(), v.data() + v.size(), value, 10);

        if (conversion_result.ec == std::errc{}) {
            assert(conversion_result.ptr == v.data() + v.size());
            return value;
        }

        // no other errors should be possible, since the pattern only admits valid integers
        assert(conversion_result.ec == std::errc::result_out_of_range);
        return std::nullopt;
    }
};

export
class TokenUnsignedReal final
    : public TokenWithCustomHR<TokenUnsignedReal>
{
public:
    using TokenWithCustomHR::TokenWithCustomHR;
    static const std::string HUMAN_REPRESENTATION;

    bool
    requiresSeparation() override { return true; }

    template <std::floating_point T>
    std::optional<T>
    value() const {
        auto v = view();
        T value;
        auto conversion_result
            = std::from_chars(v.data(), v.data() + v.size(), value);

        if (conversion_result.ec == std::errc{}) {
            assert(conversion_result.ptr == v.data() + v.size());
            return value;
        }

        // no other errors should be possible, since the pattern only admits valid reals
        assert(conversion_result.ec == std::errc::result_out_of_range);
        return std::nullopt;
    }
};

export
class TokenCharacterString final
    : public TokenWithCustomHR<TokenCharacterString>
{
public:
    using TokenWithCustomHR::TokenWithCustomHR;
    static const std::string HUMAN_REPRESENTATION;

    std::string
    value() const;
};

export
class TokenEof final : public TokenWithCustomHR<TokenEof> {
public:
    using TokenWithCustomHR::TokenWithCustomHR;
    static const std::string HUMAN_REPRESENTATION;
};

struct GrammarToken {
    using result_type = std::unique_ptr<Token>;

    template <typename T>
    static result_type
    makeResult(std::string_view view) {
        return std::make_unique<T>(view);
    }
};

struct SeparatorWhitespace {};
struct SeparatorComment {};

struct GrammarSeparator {
    using result_type = std::size_t;

    template <typename T>
    static result_type
    makeResult(std::string_view view) {
        return view.size();
    }
};
