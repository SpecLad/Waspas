module;

#include <cassert>
#include <cstdint>
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

            auto *p_token = (*reader_.tokens_it_).get();

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

    TokenReader(
        std::span<const std::unique_ptr<Token>> tokens
    )
        : tokens_it_(tokens.begin())
        , tokens_end_(tokens.end())
    {}

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
            unsuccessful_token_reprs_.clear();
            return p_next_token_typed;
        }

        unsuccessful_token_reprs_.push_back(T::HUMAN_REPRESENTATION);
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
        auto maybeInt = consume<TokenUnsignedInteger>().spelling<pascal_integer_t>();

        if (!maybeInt || *maybeInt > MAX_LABEL_VALUE) {
            --tokens_it_;
            throw InvalidLabel(*this);
        }

        return int(*maybeInt);
    }

    pascal_integer_t
    consumeInt() {
        auto maybeInt = consume<TokenUnsignedInteger>().spelling<pascal_integer_t>();

        if (!maybeInt) {
            --tokens_it_;
            throw InvalidInteger(*this);
        }

        return *maybeInt;
    }

private:
    std::string
    buildExpectedRepresentationsString() {
        std::string result(unsuccessful_token_reprs_.front());

        for (
            auto it = unsuccessful_token_reprs_.begin() + 1;
            it < unsuccessful_token_reprs_.end() - 1;
            ++it
        ) {
            result += ", ";
            result += *it;
        }

        if (unsuccessful_token_reprs_.size() > 1) {
            result += " or ";
            result += unsuccessful_token_reprs_.back();
        }

        return result;
    }

    std::span<const std::unique_ptr<Token>>::iterator tokens_it_, tokens_end_;

    std::vector<std::string_view> unsuccessful_token_reprs_;
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
    explicit Parser(TokenReader &token_reader) : token_reader_(token_reader) {}

    ViewRecorder
    viewRecorder(Node &node) {
        return ViewRecorder(node, token_reader_);
    }

    template <typename TokenSeparator, typename N>
    void
    parseSeparatedList(std::vector<N> &nodes, void (Parser::*parse_item)(N &)) {
        do {
            nodes.push_back({});
            (this->*parse_item)(nodes.back());
        }
        while (token_reader_.tryConsume<TokenSeparator>());
    }

    void
    parseLabelDeclaration(nodes::LabelDeclaration &ld) {
        auto rec = viewRecorder(ld);
        ld.value = token_reader_.consumeLabel();
    }

    void
    parseConstantDefinition(nodes::ConstantDefinition &cd) {
        auto rec = viewRecorder(cd);
        cd.name = token_reader_.consumeId();
        token_reader_.consume<TokenEqual>();

        cd.value.reset(new nodes::UnsignedIntegerConstant);
        {
            auto rec = viewRecorder(*cd.value);
            cd.value->value = token_reader_.consumeInt();
        }
        // TODO: parse other types of values
    }

    void
    parseBlock(nodes::Block &block) {
        auto rec = viewRecorder(block);

        if (token_reader_.tryConsume<TokenWsLabel>()) {
            parseSeparatedList<TokenComma>(
                block.label_declarations, &Parser::parseLabelDeclaration);
            token_reader_.consume<TokenSemicolon>();
        }

        /* TODO:
            constant-definition-part
            type-definition-part
            variable-declaration-part
            procedure-and-function-declaration-part
            statement-part
        */
    }

    void
    parseProgramParameterDeclaration(nodes::ProgramParameterDeclaration &ppd) {
        auto rec = viewRecorder(ppd);
        ppd.name = token_reader_.consumeId();
    }

    void
    parseProgram(nodes::Program &program) {
        auto rec = viewRecorder(program);

        token_reader_.consume<TokenWsProgram>();

        program.name = token_reader_.consumeId();

        if (token_reader_.tryConsume<TokenLeftParenthesis>()) {
            parseSeparatedList<TokenComma>(
                program.parameter_declarations, &Parser::parseProgramParameterDeclaration);
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
