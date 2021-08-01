module;

#include <cassert>
#include <cstdint>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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

    class InvalidDirective : public Error {
    public:
        InvalidDirective(
            TokenReader &reader,
            std::string_view expected_directive
        )
            : Error(reader), expected_directive_(expected_directive)
        {}

        void
        report(Reporter &reporter) {
            auto *p_token = (*reader_.tokens_it_).get();

            reporter.err(p_token->view().data(), "invalid-directive",
                "expected directive \"{}\"; got \"{}\" instead",
                expected_directive_, p_token->view());
        }

    private:
        std::string_view expected_directive_;
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

    // must call tryConsume at the current token at least once before this!
    const Token *
    consumeAny() {
        auto *p_next_token = (*tokens_it_).get();
        if (dynamic_cast<TokenEof *>(p_next_token)) {
            throw UnexpectedToken(*this);
        }

        ++tokens_it_;
        return p_next_token;
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

    void
    consumeDirectiveForward() {
        auto maybe_directive = consume<TokenIdentifier>().spelling();

        static constexpr std::string_view FORWARD("forward");

        if (maybe_directive != FORWARD) {
            --tokens_it_;
            throw InvalidDirective(*this, FORWARD);
        }
    }

    int
    consumeLabel() {
        auto maybe_int = consume<TokenUnsignedInteger>().value<pascal_integer_t>();

        if (!maybe_int || *maybe_int > MAX_LABEL_VALUE) {
            --tokens_it_;
            throw InvalidLabel(*this);
        }

        return int(*maybe_int);
    }

    pascal_integer_t
    consumeInt() {
        auto maybe_int = consume<TokenUnsignedInteger>().value<pascal_integer_t>();

        if (!maybe_int) {
            --tokens_it_;
            throw InvalidInteger(*this);
        }

        return *maybe_int;
    }

    pascal_real_t
    consumeReal() {
        auto maybe_real = consume<TokenUnsignedReal>().value<pascal_real_t>();

        if (!maybe_real) {
            --tokens_it_;
            throw InvalidReal(*this);
        }

        return *maybe_real;
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
    using parse_f = void (Parser::*)(N &, Args...);

    explicit Parser(TokenReader &token_reader) : token_reader_(token_reader) {}

    ViewRecorder
    viewRecorder(Node &node) {
        return ViewRecorder(node, token_reader_);
    }

    template <typename N, typename ...Args>
    bool
    tryParse(N &node, parse_f<N, Args...> parse, Args ...args) {
        auto pos = token_reader_.pos();

        try {
            (this->*parse)(node, args...);
            return true;
        }
        catch (TokenReader::UnexpectedToken &) {
            token_reader_.backtrack(pos);
            return false;
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

    template <typename N, typename Alt>
    void
    parseAlternative(std::unique_ptr<N> &node_ptr, parse_f<Alt> parse_alternative) {
        Alt node;
        (this->*parse_alternative)(node);

        if constexpr (std::is_base_of_v<Node, Alt>)
            node_ptr = std::make_unique<Alt>(std::move(node));
        else
            node_ptr = std::move(node);
    }

    template <typename N, typename Alt>
    bool
    tryParseAlternative(std::unique_ptr<N> &node_ptr, parse_f<Alt> parse_alternative) {
        return tryParse(node_ptr, &Parser::parseAlternative<N, Alt>, parse_alternative);
    }

    template <typename N, typename Alt0, typename ...Alts>
    void
    parseAlternatives(
        std::unique_ptr<N> &node_ptr,
        parse_f<Alt0> parse_alternative0,
        parse_f<Alts> ...parse_alternative
    ) {
        if constexpr (sizeof...(Alts) == 0) {
            parseAlternative(node_ptr, parse_alternative0);
        }
        else {
            if (tryParseAlternative(node_ptr, parse_alternative0))
                return;

            parseAlternatives(node_ptr, parse_alternative...);
        }
    }

    template <typename N>
    void
    parseOptional(std::optional<N> &maybe_node, parse_f<N> parse) {
        N node;
        if (tryParse(node, parse))
            maybe_node = std::move(node);
    }

    void
    parseLabel(nodes::Label &ld) {
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
        i.spelling = token_reader_.consumeId();
    }

    void
    parseSignableConstant(std::unique_ptr<nodes::SignableConstant> &sc) {
        parseAlternatives(sc,
            &Parser::parseUnsignedIntegerConstant,
            &Parser::parseUnsignedRealConstant,
            &Parser::parseIdentifier);
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

        parseSignableConstant(sc.unsigned_value);
    }

    void
    parseCharacterString(nodes::CharacterString &cs) {
        auto rec = viewRecorder(cs);
        cs.value = token_reader_.consume<TokenCharacterString>().value();
    }

    void
    parseConstant(std::unique_ptr<nodes::Constant> &c) {
        parseAlternatives(c,
            &Parser::parseSignedConstant,
            &Parser::parseSignableConstant,
            &Parser::parseCharacterString);
    }

    void
    parseConstantDefinition(nodes::ConstantDefinition &cd) {
        auto rec = viewRecorder(cd);
        parseIdentifier(cd.name);
        token_reader_.consume<TokenEqual>();
        parseConstant(cd.value);
    }

    void
    parseEnumeratedType(nodes::EnumeratedType &et) {
        auto rec = viewRecorder(et);

        token_reader_.consume<TokenLeftParenthesis>();
        parseSeparatedList<TokenComma>(
            et.constants, &Parser::parseIdentifier);
        token_reader_.consume<TokenRightParenthesis>();
    }

    void
    parseSubrangeType(nodes::SubrangeType &st) {
        auto rec = viewRecorder(st);
        parseConstant(st.smallest);
        token_reader_.consume<TokenDotDot>();
        parseConstant(st.largest);
    }

    void
    parseOrdinalType(std::unique_ptr<nodes::OrdinalType> &ot) {
        parseAlternatives(ot,
            &Parser::parseEnumeratedType,
            &Parser::parseSubrangeType,
            &Parser::parseIdentifier);
    }

    void
    parseArrayType(nodes::ArrayType &at) {
        auto rec = viewRecorder(at);
        token_reader_.consume<TokenWsArray>();
        token_reader_.consume<TokenLeftBracket>();
        parseSeparatedList<TokenComma>(at.index_types, &Parser::parseOrdinalType);
        token_reader_.consume<TokenRightBracket>();
        token_reader_.consume<TokenWsOf>();
        parseTypeDenoter(at.component_type);
    }

    void
    parseRecordSection(nodes::RecordSection &rs) {
        auto rec = viewRecorder(rs);

        parseSeparatedList<TokenComma>(rs.field_names, &Parser::parseIdentifier);
        token_reader_.consume<TokenColon>();
        parseTypeDenoter(rs.field_type);
    }

    void
    parseVariant(nodes::Variant &variant) {
        auto rec = viewRecorder(variant);

        parseSeparatedList<TokenComma>(variant.case_constants, &Parser::parseConstant);
        token_reader_.consume<TokenColon>();
        token_reader_.consume<TokenLeftParenthesis>();
        parseFieldList(variant.fields);
        token_reader_.consume<TokenRightParenthesis>();
    }

    void
    parseVariantPart(nodes::VariantPart &vp) {
        auto rec = viewRecorder(vp);

        token_reader_.consume<TokenWsCase>();

        nodes::Identifier tag_field_or_type;
        parseIdentifier(tag_field_or_type);

        if (token_reader_.tryConsume<TokenColon>()) {
            vp.tag_field = std::move(tag_field_or_type);
            parseIdentifier(vp.tag_type);
        }
        else {
            vp.tag_type = std::move(tag_field_or_type);
        }

        token_reader_.consume<TokenWsOf>();
        parseSeparatedList<TokenSemicolon>(vp.variants, &Parser::parseVariant);
    }

    void
    parseFieldList(nodes::FieldList &fl) {
        auto rec = viewRecorder(fl);

        bool has_fixed = tryParse(fl.fixed_sections,
            &Parser::parseSeparatedList<TokenSemicolon, nodes::RecordSection>,
            &Parser::parseRecordSection);

        if (!has_fixed || token_reader_.tryConsume<TokenSemicolon>()) {
            parseOptional(fl.variant_part, &Parser::parseVariantPart);
            if (fl.variant_part)
                token_reader_.tryConsume<TokenSemicolon>();
        }
    }

    void
    parseRecordType(nodes::RecordType &rt) {
        auto rec = viewRecorder(rt);

        token_reader_.consume<TokenWsRecord>();
        parseFieldList(rt.fields);
        token_reader_.consume<TokenWsEnd>();
    }

    void
    parseSetType(nodes::SetType &st) {
        auto rec = viewRecorder(st);

        token_reader_.consume<TokenWsSet>();
        token_reader_.consume<TokenWsOf>();
        parseOrdinalType(st.base_type);
    }

    void
    parseFileType(nodes::FileType &ft) {
        auto rec = viewRecorder(ft);

        token_reader_.consume<TokenWsFile>();
        token_reader_.consume<TokenWsOf>();
        parseTypeDenoter(ft.component_type);
    }

    void
    parseNewStructuredType(nodes::NewStructuredType &nst) {
        auto rec = viewRecorder(nst);
        nst.is_packed = token_reader_.tryConsume<TokenWsPacked>();
        parseAlternatives(nst.unpacked,
            &Parser::parseArrayType,
            &Parser::parseRecordType,
            &Parser::parseSetType,
            &Parser::parseFileType);
    }

    void
    parseNewPointerType(nodes::NewPointerType &npt) {
        auto rec = viewRecorder(npt);
        token_reader_.consume<TokenCaret>();
        parseIdentifier(npt.domain_type);
    }

    void
    parseTypeDenoter(std::unique_ptr<nodes::TypeDenoter> &td) {
        parseAlternatives(td,
            &Parser::parseEnumeratedType,
            &Parser::parseSubrangeType,
            &Parser::parseNewStructuredType,
            &Parser::parseNewPointerType,
            // Identifier has to come after subrange type,
            // because a subrange type can begin with an identifier.
            &Parser::parseIdentifier);
    }

    void
    parseTypeDefinition(nodes::TypeDefinition &td) {
        auto rec = viewRecorder(td);
        parseIdentifier(td.name);
        token_reader_.consume<TokenEqual>();
        parseTypeDenoter(td.denoter);
    }

    void
    parseVariableDeclaration(nodes::VariableDeclaration &vd) {
        auto rec = viewRecorder(vd);
        parseSeparatedList<TokenComma>(vd.var_names, &Parser::parseIdentifier);
        token_reader_.consume<TokenColon>();
        parseTypeDenoter(vd.var_type);
    }

    void
    parseIndexTypeSpecification(nodes::IndexTypeSpecification &its) {
        auto rec = viewRecorder(its);

        parseIdentifier(its.smallest);
        token_reader_.consume<TokenDotDot>();
        parseIdentifier(its.largest);
        token_reader_.consume<TokenColon>();
        parseIdentifier(its.bound_type);
    }

    void
    parsePackedConformantArraySchema(nodes::PackedConformantArraySchema &pcas) {
        auto rec = viewRecorder(pcas);

        token_reader_.consume<TokenWsPacked>();
        token_reader_.consume<TokenWsArray>();
        token_reader_.consume<TokenLeftBracket>();
        parseIndexTypeSpecification(pcas.index_type);
        token_reader_.consume<TokenRightBracket>();
        token_reader_.consume<TokenWsOf>();
        parseIdentifier(pcas.component_type);
    }

    void
    parseUnpackedConformantArraySchema(nodes::UnpackedConformantArraySchema &ucas) {
        auto rec = viewRecorder(ucas);

        token_reader_.consume<TokenWsArray>();
        token_reader_.consume<TokenLeftBracket>();
        parseSeparatedList<TokenSemicolon>(ucas.index_types,
            &Parser::parseIndexTypeSpecification);
        token_reader_.consume<TokenRightBracket>();
        token_reader_.consume<TokenWsOf>();
        parseFormalParameterTypeOrSchema(ucas.component_type);
    }

    void
    parseFormalParameterTypeOrSchema(std::unique_ptr<nodes::FormalParameterTypeOrSchema> &fptos) {
        parseAlternatives(fptos,
            &Parser::parseIdentifier,
            &Parser::parsePackedConformantArraySchema,
            &Parser::parseUnpackedConformantArraySchema);
    }

    void
    parseRegularParameterSection(nodes::RegularParameterSection &rps) {
        auto rec = viewRecorder(rps);
        rps.is_variable = token_reader_.tryConsume<TokenWsVar>();
        parseSeparatedList<TokenComma>(rps.parameter_names, &Parser::parseIdentifier);
        token_reader_.consume<TokenColon>();
        parseFormalParameterTypeOrSchema(rps.parameter_type);
    }

    void
    parseFormalParameterSection(std::unique_ptr<nodes::FormalParameterSection> &rps) {
        parseAlternatives(rps,
            &Parser::parseRegularParameterSection,
            &Parser::parseProcedureHeading,
            &Parser::parseFunctionHeading);
    }

    void
    parseProcedureHeading(nodes::ProcedureHeading &ph) {
        auto rec = viewRecorder(ph);

        token_reader_.consume<TokenWsProcedure>();
        parseIdentifier(ph.name);

        if (token_reader_.tryConsume<TokenLeftParenthesis>()) {
            parseSeparatedList<TokenSemicolon>(
                ph.parameters, &Parser::parseFormalParameterSection);
            token_reader_.consume<TokenRightParenthesis>();
        }
    }

    void
    parseFunctionHeading(nodes::FunctionHeading &fh) {
        auto rec = viewRecorder(fh);

        token_reader_.consume<TokenWsFunction>();
        parseIdentifier(fh.name);

        if (token_reader_.tryConsume<TokenLeftParenthesis>()) {
            parseSeparatedList<TokenSemicolon>(
                fh.parameters, &Parser::parseFormalParameterSection);
            token_reader_.consume<TokenRightParenthesis>();
        }

        token_reader_.consume<TokenColon>();
        parseIdentifier(fh.result_type);
    }

    void
    parseFunctionIdentification(nodes::FunctionIdentification &fi) {
        auto rec = viewRecorder(fi);

        token_reader_.consume<TokenWsFunction>();
        parseIdentifier(fi.name);
    }

    void
    parseSubroutineDeclaration(nodes::SubroutineDeclaration &sd) {
        auto rec = viewRecorder(sd);

        parseAlternatives(sd.heading,
            &Parser::parseProcedureHeading,
            // parseFunctionHeading has to go before parseFunctionIdentification,
            // because a function heading always starts with a function identification,
            &Parser::parseFunctionHeading,
            &Parser::parseFunctionIdentification);

        token_reader_.consume<TokenSemicolon>();

        if (dynamic_cast<nodes::FunctionIdentification *>(sd.heading.get())) {
            // function identifications can't be combined with a forward directive
            nodes::Block block;
            parseBlock(block);
            sd.block = std::move(block);
            return;
        }

        parseOptional(sd.block, &Parser::parseBlock);
        if (sd.block) return;

        token_reader_.consumeDirectiveForward();
    }

    void
    parseIndexingModifier(nodes::IndexingModifier &im) {
        auto rec = viewRecorder(im);

        token_reader_.consume<TokenLeftBracket>();
        parseSeparatedList<TokenComma>(im.indices, &Parser::parseExpression);
        token_reader_.consume<TokenRightBracket>();
    }

    void
    parseFieldAccessModifier(nodes::FieldAccessModifier &fam) {
        auto rec = viewRecorder(fam);
        token_reader_.consume<TokenDot>();
        parseIdentifier(fam.field);
    }

    void
    parseDereferencingModifier(nodes::DereferencingModifier &dm) {
        auto rec = viewRecorder(dm);
        token_reader_.consume<TokenCaret>();
    }

    void
    parseVariableModifier(std::unique_ptr<nodes::VariableModifier> &vm) {
        parseAlternatives(vm,
            &Parser::parseIndexingModifier,
            &Parser::parseFieldAccessModifier,
            &Parser::parseDereferencingModifier);
    }

    void
    parseVariableAccess(nodes::VariableAccess &va) {
        auto rec = viewRecorder(va);

        parseIdentifier(va.variable);

        for (;;) {
            std::unique_ptr<nodes::VariableModifier> modifier;

            if (!tryParse(modifier, &Parser::parseVariableModifier))
                break;

            va.modifiers.push_back(std::move(modifier));
        }
    }

    void
    parseNil(nodes::Nil &nil) {
        auto rec = viewRecorder(nil);
        token_reader_.consume<TokenWsNil>();
    }

    void
    parseParenthetical(nodes::Parenthetical &p) {
        auto rec = viewRecorder(p);
        token_reader_.consume<TokenLeftParenthesis>();
        parseExpression(p.inner_expression);
        token_reader_.consume<TokenRightParenthesis>();
    }

    void
    parseNotExpression(nodes::NotExpression &ne) {
        auto rec = viewRecorder(ne);
        token_reader_.consume<TokenWsNot>();
        parseFactor(ne.operand);
    }

    void
    parseFactor(std::unique_ptr<nodes::Factor> &factor) {
        parseAlternatives(factor,
            &Parser::parseVariableAccess,
            &Parser::parseUnsignedIntegerConstant,
            &Parser::parseUnsignedRealConstant,
            &Parser::parseCharacterString,
            &Parser::parseNil,
            /*
            &Parser::parseFunctionDesignator,
            &Parser::parseSetConstructor,
            */
            &Parser::parseParenthetical,
            &Parser::parseNotExpression);
    }

    void
    parseExpression(nodes::Expression &expression) {
        auto rec = viewRecorder(expression);
        // TODO: replace with proper parsing
        parseFactor(expression.factor);
    }

    void
    parseStatement(nodes::Statement &statement) {
        auto rec = viewRecorder(statement);

        parseOptional(statement.label, &Parser::parseLabel);
        if (statement.label) token_reader_.consume<TokenColon>();
        parseAlternatives(statement.unlabeled,
            &Parser::parseAssignmentStatement,
            // parseProcedureStatement must come after parseAssignmentStatement,
            // since it can preempt it if there are no parameters.
            &Parser::parseProcedureStatement,
            &Parser::parseGotoStatement,
            &Parser::parseCompoundStatement,
            &Parser::parseIfStatement,
            &Parser::parseCaseStatement,
            /*
            &Parser::parseRepeatStatement,
            &Parser::parseWhileStatement,
            &Parser::parseForStatement,
            &Parser::parseWithStatement,
            */
            // parseEmptyStatement must go to the bottom,
            // or it will preempt everything else.
            &Parser::parseEmptyStatement);
    }

    void
    parseAssignmentStatement(nodes::AssignmentStatement &as) {
        auto rec = viewRecorder(as);
        parseVariableAccess(as.access);
        token_reader_.consume<TokenAssign>();
        parseExpression(as.expression);
    }

    void
    parseActualParameter(nodes::ActualParameter &ap) {
        auto rec = viewRecorder(ap);

        parseExpression(ap.value);

        if (token_reader_.tryConsume<TokenColon>()) {
            ap.total_width.emplace();
            parseExpression(*ap.total_width);

            if (token_reader_.tryConsume<TokenColon>()) {
                ap.frac_digits.emplace();
                parseExpression(*ap.frac_digits);
            }
        }
    }

    void
    parseProcedureStatement(nodes::ProcedureStatement &ps) {
        auto rec = viewRecorder(ps);

        parseIdentifier(ps.procedure);

        if (token_reader_.tryConsume<TokenLeftParenthesis>()) {
            parseSeparatedList<TokenComma>(ps.parameters, &Parser::parseActualParameter);
            token_reader_.consume<TokenRightParenthesis>();
        }
    }

    void
    parseGotoStatement(nodes::GotoStatement &gs) {
        auto rec = viewRecorder(gs);
        token_reader_.consume<TokenWsGoto>();
        parseLabel(gs.label);
    }

    void
    parseIfStatement(nodes::IfStatement &is) {
        auto rec = viewRecorder(is);

        token_reader_.consume<TokenWsIf>();
        parseExpression(is.condition);
        token_reader_.consume<TokenWsThen>();
        parseStatement(is.true_branch);

        if (token_reader_.tryConsume<TokenWsElse>()) {
            is.false_branch.emplace();
            parseStatement(*is.false_branch);
        }
    }

    void
    parseCaseListElement(nodes::CaseListElement &cle) {
        auto rec = viewRecorder(cle);

        parseSeparatedList<TokenComma>(cle.constants, &Parser::parseConstant);
        token_reader_.consume<TokenColon>();
        parseStatement(cle.statement);
    }

    void
    parseCaseStatement(nodes::CaseStatement &cs) {
        auto rec = viewRecorder(cs);

        token_reader_.consume<TokenWsCase>();
        parseExpression(cs.case_index);
        token_reader_.consume<TokenWsOf>();
        parseSeparatedList<TokenSemicolon>(cs.cases, &Parser::parseCaseListElement);
        token_reader_.tryConsume<TokenSemicolon>();
        token_reader_.consume<TokenWsEnd>();
    }

    void
    parseEmptyStatement(nodes::EmptyStatement &es) {
        auto rec = viewRecorder(es);
    }

    void
    parseCompoundStatement(nodes::CompoundStatement &cs) {
        auto rec = viewRecorder(cs);

        token_reader_.consume<TokenWsBegin>();

        parseSeparatedList<TokenSemicolon>(cs.statements, &Parser::parseStatement);

        // TODO: remove this
        int nesting_level = 1;
        do {
            if (token_reader_.tryConsume<TokenWsBegin>()) ++nesting_level;
            else if (token_reader_.tryConsume<TokenWsCase>()) ++nesting_level;
            else if (token_reader_.tryConsume<TokenWsEnd>()) --nesting_level;
            else token_reader_.consumeAny();
        }
        while (nesting_level > 0);
    }

    void
    parseBlock(nodes::Block &block) {
        auto rec = viewRecorder(block);

        if (token_reader_.tryConsume<TokenWsLabel>()) {
            parseSeparatedList<TokenComma>(
                block.label_declarations, &Parser::parseLabel);
            token_reader_.consume<TokenSemicolon>();
        }

        if (token_reader_.tryConsume<TokenWsConst>()) {
            parseSeparatedList<TokenSemicolon>(
                block.constant_definitions, &Parser::parseConstantDefinition);
            token_reader_.consume<TokenSemicolon>();
        }

        if (token_reader_.tryConsume<TokenWsType>()) {
            parseSeparatedList<TokenSemicolon>(
                block.type_definitions, &Parser::parseTypeDefinition);
            token_reader_.consume<TokenSemicolon>();
        }

        if (token_reader_.tryConsume<TokenWsVar>()) {
            parseSeparatedList<TokenSemicolon>(
                block.variable_declarations, &Parser::parseVariableDeclaration);
            token_reader_.consume<TokenSemicolon>();
        }

        if (tryParse(
            block.subroutine_declarations,
            &Parser::parseSeparatedList<TokenSemicolon, nodes::SubroutineDeclaration>,
            &Parser::parseSubroutineDeclaration)
        ) {
            token_reader_.consume<TokenSemicolon>();
        }

        parseCompoundStatement(block.statement);
    }

    void
    parseProgram(nodes::Program &program) {
        auto rec = viewRecorder(program);

        token_reader_.consume<TokenWsProgram>();

        parseIdentifier(program.name);

        if (token_reader_.tryConsume<TokenLeftParenthesis>()) {
            parseSeparatedList<TokenComma>(
                program.parameter_declarations, &Parser::parseIdentifier);
            token_reader_.consume<TokenRightParenthesis>();
        }

        token_reader_.consume<TokenSemicolon>();

        parseBlock(program.block);

        token_reader_.consume<TokenDot>();
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
        token_reader.consume<TokenEof>();
    }
    catch (TokenReader::Error &e) {
        e.report(reporter);
    }

    return program;
}
