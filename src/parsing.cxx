module;

#include <optional>
#include <span>

#include <typeinfo> // TODO: remove this

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
    const T *expect() {
        auto *p_next_token = (*tokens_it_++).get();

        if (auto *p_next_token_typed = dynamic_cast<T *>(p_next_token))
            return p_next_token_typed;

        reporter_.err(p_next_token->view().data(), "unexpected-token",
            "expected a token of type {}, got {} instead",
            T::HUMAN_REPRESENTATION, p_next_token->humanRepresentation());

        return nullptr;
    }

private:
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

    if (!token_reader.expect<TokenWsProgram>())
        return program;

    if (auto id = token_reader.expect<TokenIdentifier>())
        program.name = id->spelling();

    return program;
}
