module;

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

module parsing;

class UnexpectedToken {
public:
    using get_reprs_str_f = std::string(*)();

    UnexpectedToken(Token *token, get_reprs_str_f get_reprs_str)
        : token_(token), get_reprs_str_(get_reprs_str)
    {}

    void
    report(Reporter &reporter) {
        reporter.err(token_->view().data(), "unexpected-token",
            "expected a token of type {}, got {} instead",
            get_reprs_str_(), token_->humanRepresentation());
    }

private:
    Token *token_;
    get_reprs_str_f get_reprs_str_;
};

class TokenReader {
public:
    TokenReader(
        std::span<const std::unique_ptr<Token>> tokens
    )
        : tokens_it_(tokens.begin())
        , tokens_end_(tokens.end())
    {}

    template <typename T>
    const T &
    consume() {
        auto *p_next_token = (*tokens_it_++).get();

        if (auto *p_next_token_typed = dynamic_cast<T *>(p_next_token))
            return *p_next_token_typed;

        throw UnexpectedToken(p_next_token, &buildExpectedRepresentationsString<T>);
    }

    template <typename ...Ts>
    std::variant<Ts *...>
    consumeOneOf() {
        auto *p_next_token = (*tokens_it_++).get();

        std::variant<Ts *...> result;
        if (tryCastToken<Ts...>(p_next_token, result))
            return result;

        throw UnexpectedToken(p_next_token, &buildExpectedRepresentationsString<Ts...>);
    }


private:
    template <typename T0, typename ...Ts, typename Dest>
    bool
    tryCastToken(Token *p_token, Dest &destination) {
        if (auto *p_token_typed = dynamic_cast<T0 *>(p_token)) {
            destination = p_token_typed;
            return true;
        }

        if constexpr (sizeof...(Ts) == 0)
            return false;
        else
            return tryCastToken<Ts...>(p_token, destination);
    }

    template <typename ...Ts>
    static std::string
    buildExpectedRepresentationsString() {
        static_assert(sizeof...(Ts) >= 1);
        return buildExpectedRepresentationsStringHelper({ Ts::HUMAN_REPRESENTATION... });
    }

    static std::string
    buildExpectedRepresentationsStringHelper(
        std::initializer_list<std::string_view> reprs
    ) {
        std::string result(*reprs.begin());

        for (auto it = reprs.begin() + 1; it < reprs.end() - 1; ++it) {
            result += ", ";
            result += *it;
        }

        if (reprs.size() > 1) {
            result += " or ";
            result += *(reprs.end() - 1);
        }

        return result;
    }

    std::span<const std::unique_ptr<Token>>::iterator tokens_it_, tokens_end_;
};

void
parseProgram(TokenReader &token_reader, NodeProgram &program) {
    token_reader.consume<TokenWsProgram>();

    program.name = token_reader.consume<TokenIdentifier>().spelling();

    auto next_token = token_reader.consumeOneOf<TokenLeftParenthesis, TokenSemicolon>();

    if (std::holds_alternative<TokenLeftParenthesis *>(next_token)) {
        program.parameter_declarations.push_back({});
        program.parameter_declarations.back().name
            = token_reader.consume<TokenIdentifier>().spelling();

        for (;;) {
            auto next_token = token_reader.consumeOneOf<TokenComma, TokenRightParenthesis>();

            if (std::holds_alternative<TokenComma *>(next_token)) {
                program.parameter_declarations.push_back({});
                program.parameter_declarations.back().name
                    = token_reader.consume<TokenIdentifier>().spelling();
            }
            else if (std::holds_alternative<TokenRightParenthesis *>(next_token)) {
                break;
            }
        }

        token_reader.consume<TokenSemicolon>();
    }
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
    catch (UnexpectedToken &ut) {
        ut.report(reporter);
    }

    return program;
}
