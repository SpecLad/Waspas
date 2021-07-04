module;

#include <cassert>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

module parsing;

constexpr std::uint32_t MAX_LABEL_VALUE = 9999;

class TokenReader {
public:
    struct UnexpectedToken {};
    struct InvalidLabel {};

    TokenReader(
        std::span<const std::unique_ptr<Token>> tokens
    )
        : tokens_it_(tokens.begin())
        , tokens_end_(tokens.end())
    {}

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

        throw UnexpectedToken();
    }

    std::string
    consumeId() {
        return consume<TokenIdentifier>().spelling();
    }

    int
    consumeLabel() {
        auto maybeInt = consume<TokenUnsignedInteger>().spelling<std::uint32_t>();

        if (!maybeInt || *maybeInt > MAX_LABEL_VALUE) {
            --tokens_it_;
            throw InvalidLabel();
        }

        return *maybeInt;
    }

    void
    reportUnexpectedToken(Reporter &reporter) {
        assert(!unsuccessful_token_reprs_.empty());

        auto *p_token = (*tokens_it_).get();

        reporter.err(p_token->view().data(), "unexpected-token",
            "expected a token of type {}, got {} instead",
            buildExpectedRepresentationsString(), p_token->humanRepresentation());
    }

    void
    reportInvalidLabel(Reporter &reporter) {
        auto *p_token = (*tokens_it_).get();

        reporter.err(p_token->view().data(), "invalid-label",
            "label value {} is too large; maximum is {}",
            p_token->view(), MAX_LABEL_VALUE);
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

class Parser {
public:
    explicit Parser(TokenReader &token_reader) : token_reader_(token_reader) {}

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
        ld.value = token_reader_.consumeLabel();
    }

    void
    parseBlock(nodes::Block &block) {
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
        ppd.name = token_reader_.consumeId();
    }

    void
    parseProgram(nodes::Program &program) {
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
    catch (TokenReader::UnexpectedToken &) {
        token_reader.reportUnexpectedToken(reporter);
    }
    catch (TokenReader::InvalidLabel &) {
        token_reader.reportInvalidLabel(reporter);
    }

    return program;
}
