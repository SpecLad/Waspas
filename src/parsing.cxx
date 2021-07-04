module;

#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

module parsing;

class TokenReader {
public:
    TokenReader(
        std::span<const std::unique_ptr<Token>> tokens,
        Reporter &reporter
    )
        : tokens_it_(tokens.begin())
        , tokens_end_(tokens.end())
        , reporter_(reporter)
    {}

    template <typename T>
    const T *
    consume() {
        auto *p_next_token = (*tokens_it_++).get();

        if (auto *p_next_token_typed = dynamic_cast<T *>(p_next_token))
            return p_next_token_typed;

        reporter_.err(p_next_token->view().data(), "unexpected-token",
            "expected a token of type {}, got {} instead",
            T::HUMAN_REPRESENTATION, p_next_token->humanRepresentation());

        return nullptr;
    }

    template <typename ...Ts>
    std::variant<std::monostate, Ts *...>
    consumeOneOf() {
        auto *p_next_token = (*tokens_it_++).get();

        std::variant<std::monostate, Ts *...> result;
        tryCastToken<Ts...>(p_next_token, result);

        if (!std::holds_alternative<std::monostate>(result))
            return result;

        static_assert(sizeof...(Ts) >= 1);

        reporter_.err(p_next_token->view().data(), "unexpected-token",
            "expected a token of type {}, got {} instead",
            buildExpectedRepresentationsString({ Ts::HUMAN_REPRESENTATION... }),
            p_next_token->humanRepresentation());

        return result;
    }


private:
    template <typename T0, typename ...Ts, typename Dest>
    void
    tryCastToken(Token *p_token, Dest &destination) {
        if (auto *p_token_typed = dynamic_cast<T0 *>(p_token)) {
            destination = p_token_typed;
            return;
        }

        if constexpr (sizeof...(Ts) == 0) {
            return;
        }
        else {
            tryCastToken<Ts...>(p_token, destination);
        }
    }

    std::string
    buildExpectedRepresentationsString(std::initializer_list<std::string> reprs) {
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
    Reporter &reporter_;
};

NodeProgram
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
) {
    TokenReader token_reader(tokens, reporter);

    NodeProgram program;

    if (!token_reader.consume<TokenWsProgram>())
        return program;

    {
        auto p_id = token_reader.consume<TokenIdentifier>();
        if (!p_id) return program;

        program.name = p_id->spelling();
    }

    auto next_token = token_reader.consumeOneOf<TokenLeftParenthesis, TokenSemicolon>();

    if (std::holds_alternative<TokenLeftParenthesis *>(next_token)) {
        {
            program.parameter_declarations.push_back({});

            auto p_id = token_reader.consume<TokenIdentifier>();
            if (!p_id) return program;

            program.parameter_declarations.back().name = p_id->spelling();
        }

        for (;;) {
            auto next_token = token_reader.consumeOneOf<TokenComma, TokenRightParenthesis>();

            if (std::holds_alternative<TokenComma *>(next_token)) {
                program.parameter_declarations.push_back(NodeProgramParameterDeclaration{});

                auto p_id = token_reader.consume<TokenIdentifier>();
                if (!p_id) return program;

                program.parameter_declarations.back().name = p_id->spelling();
            }
            else if (std::holds_alternative<TokenRightParenthesis *>(next_token)) {
                break;
            }
            else {
                return program;
            }
        }

        if (!token_reader.consume<TokenSemicolon>())
            return program;
    }
    else if (std::holds_alternative<std::monostate>(next_token)) {
        return program;
    }

    return program;
}
