module;

#include <cassert>
#include <cstdint>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

module parsing;

constexpr pascal_integer_t MAX_LABEL_VALUE = 9999;

class TokenReader {
public:
    class Error {
    public:
        explicit
        Error(TokenReader &reader) : reader_(reader) {}

        virtual
        ~Error() = default;

        virtual void
        report(Reporter &reporter) = 0;

    protected:
        TokenReader &reader_;
    };

    class UnexpectedToken : public Error {
    public:
        using Error::Error;

        void
        report(Reporter &reporter) {
            assert(!reader_.unsuccessful_token_reprs_.empty());

            auto *p_token = (*reader_.unexpected_token_it_).get();

            reporter.err(p_token->view().data(), "unexpected-token",
                "expected a token of type {}, got {} instead",
                reader_.buildExpectedRepresentationsString(), p_token->humanRepresentation());
        }
    };

    class InvalidLabel : public Error {
    public:
        using Error::Error;

        void
        report(Reporter &reporter) {
            auto *p_token = (*reader_.tokens_it_).get();

            reporter.err(p_token->view().data(), "invalid-label",
                "label value {} is too large; maximum is {}",
                p_token->view(), MAX_LABEL_VALUE);
        }
    };

    class InvalidInteger : public Error {
    public:
        using Error::Error;

        void
            report(Reporter &reporter) {
            auto *p_token = (*reader_.tokens_it_).get();

            reporter.err(p_token->view().data(), "invalid-integer",
                "integer value {} is too large; maximum is {}",
                p_token->view(), std::numeric_limits<pascal_integer_t>::max());
        }
    };

    class InvalidReal : public Error {
    public:
        using Error::Error;

        void
        report(Reporter &reporter) {
            auto *p_token = (*reader_.tokens_it_).get();

            reporter.err(p_token->view().data(), "invalid-real",
                "real value {} is not in the representable range; minimum is {}, maximum is {}",
                p_token->view(),
                std::numeric_limits<pascal_real_t>::denorm_min(),
                std::numeric_limits<pascal_real_t>::max());
        }
    };

    using token_pos_t = std::span<const std::unique_ptr<Token>>::iterator;

    TokenReader(
        std::span<const std::unique_ptr<Token>> tokens
    )
        : tokens_it_(tokens.begin())
        , tokens_end_(tokens.end())
        , unexpected_token_it_(tokens.begin())
    {}

    token_pos_t
    pos() const { return tokens_it_; }

    void
    backtrack(token_pos_t pos) {
        assert(pos <= tokens_it_);
        tokens_it_ = pos;
    }

    Token *
    currentToken() const {
        return (*tokens_it_).get();
    }

    Token *
    previousToken() const {
        return (*(tokens_it_ - 1)).get();
    }

    template <typename T>
    const T *
    tryConsume() {
        auto *p_next_token = (*tokens_it_).get();

        if (auto *p_next_token_typed = dynamic_cast<T *>(p_next_token)) {
            ++tokens_it_;
            return p_next_token_typed;
        }

        if (tokens_it_ >= unexpected_token_it_) {
            if (tokens_it_ > unexpected_token_it_) {
                unsuccessful_token_reprs_.clear();
                unexpected_token_it_ = tokens_it_;
            }
            unsuccessful_token_reprs_.insert(T::HUMAN_REPRESENTATION);
        }
        return nullptr;
    }

    template <typename T>
    const T &
    consume() {
        if (auto *p_next_token_typed = tryConsume<T>())
            return *p_next_token_typed;

        throw UnexpectedToken(*this);
    }

    std::string
    consumeId() {
        return consume<TokenIdentifier>().spelling();
    }

    int
    consumeLabel() {
        auto maybeInt = consume<TokenUnsignedInteger>().value<pascal_integer_t>();

        if (!maybeInt || *maybeInt > MAX_LABEL_VALUE) {
            --tokens_it_;
            throw InvalidLabel(*this);
        }

        return int(*maybeInt);
    }

    pascal_integer_t
    consumeInt() {
        auto maybeInt = consume<TokenUnsignedInteger>().value<pascal_integer_t>();

        if (!maybeInt) {
            --tokens_it_;
            throw InvalidInteger(*this);
        }

        return *maybeInt;
    }

    pascal_real_t
    consumeReal() {
        auto maybeReal = consume<TokenUnsignedReal>().value<pascal_real_t>();

        if (!maybeReal) {
            --tokens_it_;
            throw InvalidReal(*this);
        }

        return *maybeReal;
    }

private:
    std::string
    buildExpectedRepresentationsString() {
        auto first_it = unsuccessful_token_reprs_.begin();
        std::string result(*first_it);

        auto last_it = unsuccessful_token_reprs_.end();
        --last_it;

        if (unsuccessful_token_reprs_.size() > 2) {
            auto it = first_it;
            ++it;

            for (; it != last_it; ++it) {
                result += ", ";
                result += *it;
            }
        }

        if (unsuccessful_token_reprs_.size() > 1) {
            result += " or ";
            result += *last_it;
        }

        return result;
    }

    token_pos_t tokens_it_, tokens_end_;

    token_pos_t unexpected_token_it_;
    std::set<std::string_view> unsuccessful_token_reprs_;
};

class ViewRecorder {
public:
    explicit ViewRecorder(Node &node, TokenReader &token_reader)
        : node_(node)
        , token_reader_(token_reader)
        , p_initial_token_(token_reader.currentToken())
    {
        const char *p_begin = p_initial_token_->view().data();
        node_.view = { p_begin, p_begin };
    }

    ViewRecorder(const ViewRecorder &) = delete;
    ViewRecorder &
    operator=(const ViewRecorder &) = delete;

    ~ViewRecorder() {
        if (p_initial_token_ == token_reader_.currentToken())
            return;

        std::string_view previous_token_view = token_reader_.previousToken()->view();
        const char *p_end = previous_token_view.data() + previous_token_view.size();
        node_.view = { node_.view.data(), p_end };
    }

private:
    Node &node_;
    TokenReader &token_reader_;
    const Token *p_initial_token_;
};

class Parser {
public:
    template <typename N>
    using parse_f = void (Parser::*)(N &);

    explicit Parser(TokenReader &token_reader) : token_reader_(token_reader) {}

    ViewRecorder
    viewRecorder(Node &node) {
        return ViewRecorder(node, token_reader_);
    }

    template <typename TokenSeparator, typename N>
    void
    parseSeparatedList(std::vector<N> &nodes, parse_f<N> parse_item) {
        {
            N node;
            (this->*parse_item)(node);
            nodes.push_back(std::move(node));
        }

        for (;;) {
            auto pos = token_reader_.pos();

            if (!token_reader_.tryConsume<TokenSeparator>()) break;

            try {
                N node;
                (this->*parse_item)(node);
                nodes.push_back(std::move(node));
            }
            catch (TokenReader::UnexpectedToken &) {
                token_reader_.backtrack(pos);
                return;
            }
        }
    }

    template <typename N, typename B>
    void
    parseAlternative(std::unique_ptr<B> &node_ptr, parse_f<N> parse_alternative) {
        N node;
        (this->*parse_alternative)(node);
        node_ptr = std::make_unique<N>(std::move(node));
    }

    template <typename N, typename B>
    bool
    tryParseAlternative(std::unique_ptr<B> &node_ptr, parse_f<N> parse_alternative) {
        auto pos = token_reader_.pos();

        try {
            parseAlternative(node_ptr, parse_alternative);
            return true;
        }
        catch (TokenReader::UnexpectedToken &) {
            token_reader_.backtrack(pos);
            return false;
        }
    }

    template <typename B, typename N0, typename ...Ns>
    void
    parseAlternatives(
        std::unique_ptr<B> &node_ptr,
        parse_f<N0> parse_alternative0, parse_f<Ns> ...parse_alternative
    ) {
        if constexpr (sizeof...(Ns) == 0) {
            parseAlternative(node_ptr, parse_alternative0);
        }
        else {
            if (tryParseAlternative(node_ptr, parse_alternative0))
                return;

            parseAlternatives(node_ptr, parse_alternative...);
        }
    }

    void
    parseLabelDeclaration(nodes::LabelDeclaration &ld) {
        auto rec = viewRecorder(ld);
        ld.value = token_reader_.consumeLabel();
    }

    void
    parseUnsignedIntegerConstant(nodes::UnsignedIntegerConstant &usc) {
        auto rec = viewRecorder(usc);
        usc.value = token_reader_.consumeInt();
    }

    void
    parseUnsignedRealConstant(nodes::UnsignedRealConstant &urc) {
        auto rec = viewRecorder(urc);
        urc.value = token_reader_.consumeReal();
    }

    void
    parseIdentifier(nodes::Identifier &i) {
        auto rec = viewRecorder(i);
        i.name = token_reader_.consumeId();
    }

    void
    parseSignedConstant(nodes::SignedConstant &sc) {
        auto rec = viewRecorder(sc);

        if (token_reader_.tryConsume<TokenPlus>()) {
            sc.sign = PascalSign::PLUS;
        }
        else {
            token_reader_.consume<TokenMinus>();
            sc.sign = PascalSign::MINUS;
        }

        parseAlternatives(sc.unsigned_value,
            &Parser::parseUnsignedIntegerConstant,
            &Parser::parseUnsignedRealConstant,
            &Parser::parseIdentifier);
    }

    void
    parseCharacterString(nodes::CharacterString &cs) {
        auto rec = viewRecorder(cs);
        cs.value = token_reader_.consume<TokenCharacterString>().value();
    }

    void
    parseConstantDefinition(nodes::ConstantDefinition &cd) {
        auto rec = viewRecorder(cd);
        cd.name = token_reader_.consumeId();
        token_reader_.consume<TokenEqual>();

        parseAlternatives(cd.value,
            &Parser::parseSignedConstant,
            &Parser::parseUnsignedIntegerConstant,
            &Parser::parseUnsignedRealConstant,
            &Parser::parseIdentifier,
            &Parser::parseCharacterString);
    }

    void
    parseBlock(nodes::Block &block) {
        auto rec = viewRecorder(block);

        if (token_reader_.tryConsume<TokenWsLabel>()) {
            parseSeparatedList<TokenComma>(
                block.label_declarations, &Parser::parseLabelDeclaration);
            token_reader_.consume<TokenSemicolon>();
        }

        if (token_reader_.tryConsume<TokenWsConst>()) {
            parseSeparatedList<TokenSemicolon>(
                block.constant_definitions, &Parser::parseConstantDefinition);
            token_reader_.consume<TokenSemicolon>();
        }

        /* TODO:
            type-definition-part
            variable-declaration-part
            procedure-and-function-declaration-part
            statement-part
        */
    }

    void
    parseProgram(nodes::Program &program) {
        auto rec = viewRecorder(program);

        token_reader_.consume<TokenWsProgram>();

        program.name = token_reader_.consumeId();

        if (token_reader_.tryConsume<TokenLeftParenthesis>()) {
            parseSeparatedList<TokenComma>(
                program.parameter_declarations, &Parser::parseIdentifier);
            token_reader_.consume<TokenRightParenthesis>();
        }

        token_reader_.consume<TokenSemicolon>();

        parseBlock(program.block);

        // TODO: token_reader_.consume<TokenDot>();
    }

private:
    TokenReader &token_reader_;
};

nodes::Program
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
) {
    TokenReader token_reader(tokens);
    Parser parser(token_reader);

    nodes::Program program;

    try {
        parser.parseProgram(program);
        // TODO: token_reader.consume<TokenEof>();
    }
    catch (TokenReader::Error &e) {
        e.report(reporter);
    }

    return program;
}
