#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>

import io;
import lexing;
import parsing;
import reporting;

void
dumpAstHelper(
    const Node &root, int indent,
    const char *source_start, LineIndexer &line_indexer
) {
    static constexpr int INDENT_SIZE = 4;

    Locus locus_start = line_indexer.getLocusForOffset(
        root.view.data() - source_start);
    Locus locus_end = line_indexer.getLocusForOffset(
        root.view.data() + root.view.size() - source_start);

    printError("{}({}:{}-{}:{})", root.type(),
        locus_start.line() + 1, locus_start.column() + 1,
        locus_end.line() + 1, locus_end.column() + 1);

    class FieldDumper : public NodeFieldReceiver {
    public:
        explicit FieldDumper(
            int indent,
            const char *source_start,
            LineIndexer &line_indexer
        )
            : indent(indent)
            , source_start(source_start)
            , line_indexer(line_indexer)
        {}

        void
        printFieldName(std::string_view name) {
            if (first) {
                printError(":");
                first = false;
            }

            printError("\n{:{}}{} = ", "", indent + INDENT_SIZE, name);
        }

        void
        receiveIdField(std::string_view name, std::string_view value) override {
            printFieldName(name);
            printError("{}", value);
        }

        void
        receiveIntField(std::string_view name, int value) override {
            printFieldName(name);
            printError("{}", value);
        }

        void
        receiveNodeField(std::string_view name, const Node &value) {
            printFieldName(name);
            dumpAstHelper(value, indent + INDENT_SIZE, source_start, line_indexer);
        }

        void
        receiveNodeListField(std::string_view name, std::span<const Node *> value) {
            printFieldName(name);
            printError("[");

            for (const auto &p_node : value) {
                printError("\n{:{}}", "", indent + INDENT_SIZE * 2);
                dumpAstHelper(*p_node, indent + INDENT_SIZE * 2,
                    source_start, line_indexer);
            }

            if (!value.empty())
                printError("\n{:{}}", "", indent + INDENT_SIZE);

            printError("]");
        }

        int indent;
        bool first = true;
        const char *source_start;
        LineIndexer &line_indexer;
    } dumper(indent, source_start, line_indexer);

    root.describeFields(dumper);
}

void
dumpAst(const Node &root, const char *source_start, LineIndexer &line_indexer) {
    dumpAstHelper(root, 0, source_start, line_indexer);
    printError("\n");
}

int
main(int, char **argv) {
    char **pp_arg = argv + 1;

    if (*pp_arg) {
        if (!std::strcmp(*pp_arg, "--")) {
            ++pp_arg;
        }
        else if (**pp_arg == '-') {
            printError("unknown option: {}\n", *pp_arg);
            return 1;
        }
    }

    if (!pp_arg[0] || pp_arg[1]) {
        printError("usage: {} source.pas\n", argv[0]);
        return 1;
    }

    std::filesystem::path source_path(pp_arg[0]);
    std::string source_text;

    {
        std::ifstream source_file;
        source_file.exceptions(std::ios::failbit | std::ios::badbit);

        try {
            source_file.open(source_path, std::ios::binary);
        }
        catch (std::system_error& e) {
            printError("{}: unable to open file ({})\n",
                source_path.string(), e.code().message());
            return 1;
        }

        try {
            source_text.assign(
                std::istreambuf_iterator<char>(source_file),
                std::istreambuf_iterator<char>());
        }
        catch (std::system_error& e) {
            printError("{}: unable to read file ({})\n",
                source_path.string(), e.code().message());
            return 1;
        }
    }

    LineIndexer line_indexer(source_text);
    Reporter reporter(source_path, source_text.data(), line_indexer);

    {
        auto non_ascii_it = std::find_if(source_text.begin(), source_text.end(),
            [](char c) { return (unsigned char)c >= 0x80; });

        if (non_ascii_it != source_text.end()) {
            reporter.err(&*non_ascii_it, "non-ascii-char",
                "non-ASCII character (first byte is {:#x})", (unsigned char)*non_ascii_it);
            return 1;
        }
    }

    std::vector<std::unique_ptr<Token>> tokens = lex(source_text, reporter);

    if (reporter.hadErrors())
        return 1;

    auto ast = parse(tokens, reporter);

    if (reporter.hadErrors())
        return 1;

    dumpAst(ast, source_text.data(), line_indexer);

    return 0;
}
