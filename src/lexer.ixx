module;

#include <cassert>
#include <cstdint>
#include <string>

export module lexer;

using int32 = std::int32_t;

export
class Locus {
public:
    Locus(int32 line, int32 column)
        : line_(line)
        , column_(column)
    {
        assert(line_ >= 0);
        assert(column_ >= 0);
    }

    int32 line() const { return line_; }
    int32 column() const { return column_; }

private:
    int32 line_;
    int32 column_;
};

export
class Token {
public:
    Token(const Locus &locus, int32 length) : locus_(locus), length_(length) {
        assert(length_ > 0);
    }

    const Locus &locus() const { return locus_; }
    int32 length() const { return length_; }

    virtual ~Token() = default;

private:
    Locus locus_;
    int32 length_;
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
