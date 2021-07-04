module;

#include <cassert>
#include <span>
#include <string>
#include <string_view>
#include <vector>

module parsing;

class TokenReader {
public:
    struct UnexpectedToken {};

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

    void
    reportUnexpectedToken(Reporter &reporter) {
        assert(!unsuccessful_token_reprs_.empty());

        auto *p_token = (*tokens_it_).get();

        reporter.err(p_token->view().data(), "unexpected-token",
            "expected a token of type {}, got {} instead",
            buildExpectedRepresentationsString(), p_token->humanRepresentation());
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

void
parseProgram(TokenReader &token_reader, NodeProgram &program) {
    token_reader.consume<TokenWsProgram>();

    program.name = token_reader.consume<TokenIdentifier>().spelling();

    if (token_reader.tryConsume<TokenLeftParenthesis>()) {
        program.parameter_declarations.push_back({});
        program.parameter_declarations.back().name
            = token_reader.consume<TokenIdentifier>().spelling();

        while (token_reader.tryConsume<TokenComma>()) {
            program.parameter_declarations.push_back({});
            program.parameter_declarations.back().name
                = token_reader.consume<TokenIdentifier>().spelling();
        }

        token_reader.consume<TokenRightParenthesis>();
    }

    token_reader.consume<TokenSemicolon>();
}

NodeProgram
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
) {
    TokenReader token_reader(tokens);

    NodeProgram program;

    try {
        parseProgram(token_reader, program);
    }
    catch (TokenReader::UnexpectedToken &ut) {
        token_reader.reportUnexpectedToken(reporter);
    }

    return program;
}
