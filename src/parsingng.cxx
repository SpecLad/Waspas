// SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
//
// SPDX-License-Identifier: MPL-2.0

module;

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module parsingng;

import utilities;

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

            reporter.err(p_token->view().data(), ec::UNEXPECTED_TOKEN,
                "expected a token of type {}, got {} instead",
                reader_.buildExpectedRepresentationsString(), p_token->humanRepresentation());
        }
    };

    class InvalidDirective : public Error {
    public:
        InvalidDirective(
            TokenReader &reader,
            const Cisref &expected_directive
        )
            : Error(reader), expected_directive_(expected_directive)
        {}

        void
        report(Reporter &reporter) {
            auto *p_token = (*reader_.tokens_it_).get();

            reporter.err(p_token->view().data(), ec::INVALID_DIRECTIVE,
                "expected directive \"{}\"; got \"{}\" instead",
                expected_directive_, p_token->view());
        }

    private:
        const Cisref &expected_directive_;
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

    Cisref
    consumeId() {
        return consume<TokenIdentifier>().spelling();
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
    template <typename N, typename ...Args>
    using parse_f = std::unique_ptr<N> (Parser::*)(Args...);

    explicit Parser(TokenReader &token_reader) : token_reader_(token_reader) {}

    ViewRecorder
    viewRecorder(Node &node) {
        return ViewRecorder(node, token_reader_);
    }

    template <typename N, typename ...Args>
    std::unique_ptr<Node>
    tryParse(parse_f<N, Args...> parse, Args ...args) {
        auto pos = token_reader_.pos();

        try {
            return (this->*parse)(args...);
        }
        catch (TokenReader::UnexpectedToken &) {
            token_reader_.backtrack(pos);
            return nullptr;
        }
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

    template<typename Token>
    std::unique_ptr<Node>
    parseSpecialSymbol() {
        auto node = std::make_unique<Node>();
        auto rec = viewRecorder(*node);
        token_reader_.consume<Token>();
        return node;
    }

    std::unique_ptr<Node>
    parseCharacterString() {
        return parseSpecialSymbol<TokenCharacterString>();
    }

    std::unique_ptr<Node>
    parseDirective() {
        return parseSpecialSymbol<TokenIdentifier>();
    }

    std::unique_ptr<Node>
    parseIdentifier() {
        return parseSpecialSymbol<TokenIdentifier>();
    }

    std::unique_ptr<Node>
    parseLabel() {
        return parseSpecialSymbol<TokenUnsignedInteger>();
    }

    std::unique_ptr<Node>
    parseUnsignedInteger() {
        return parseSpecialSymbol<TokenUnsignedInteger>();
    }

    std::unique_ptr<Node>
    parseUnsignedReal() {
        return parseSpecialSymbol<TokenUnsignedReal>();
    }

    template <typename Alt0, typename ...Alts>
    std::unique_ptr<Node>
    parseAlternation(
        parse_f<Alt0> parse_alternative0,
        parse_f<Alts> ...parse_alternative
    ) {
        if constexpr (sizeof...(Alts) == 0) {
            return (this->*parse_alternative0)();
        }
        else {
            if (auto node = tryParse(parse_alternative0))
                return node;

            return parseAlternation(parse_alternative...);
        }
    }

    template <typename ...Parts>
    std::unique_ptr<Node>
    parseConcatenation(
        parse_f<Parts> ...parse_part
    ) {
        auto node = std::make_unique<Node>();
        auto rec = viewRecorder(*node);

        (node->subnodes.push_back((this->*parse_part)()), ...);
        return node;
    }

    template <typename N>
    std::unique_ptr<Node>
    parseOptionality(parse_f<N> parse_item) {
        auto node = std::make_unique<Node>();
        auto rec = viewRecorder(*node);

        if (auto subnode = tryParse(parse_item))
            node->subnodes.push_back(std::move(subnode));
        return node;
    }

    template <typename N>
    std::unique_ptr<Node>
    parseRepetition(parse_f<N> parse_item) {
        auto node = std::make_unique<Node>();
        auto rec = viewRecorder(*node);

        for (;;) {
            if (auto subnode = tryParse(parse_item))
                node->subnodes.push_back(std::move(subnode));
            else
                break;
        }

        return node;
    }

#include "parsingng-gen.inc"

private:
    TokenReader &token_reader_;
};

std::unique_ptr<Node>
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
) {
    TokenReader token_reader(tokens);
    Parser parser(token_reader);

    std::unique_ptr<Node> program;

    try {
        program = parser.parseProgram();
        token_reader.consume<TokenEof>();
    }
    catch (TokenReader::Error &e) {
        e.report(reporter);
    }

    return program;
}
