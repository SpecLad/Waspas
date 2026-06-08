// SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
//
// SPDX-License-Identifier: MPL-2.0

#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>

import io;
import lexing;
import parsing;
import reporting;
import semantics;

void
dumpParseTreeHelper(
    const Node &root, int indent,
    const char *source_start, LineIndexer &line_indexer
) {
    static constexpr int INDENT_SIZE = 4;

    Locus locus_start = line_indexer.getLocusForOffset(
        root.view.data() - source_start);
    Locus locus_end = line_indexer.getLocusForOffset(
        root.view.data() + root.view.size() - source_start);

    printOutput("{}({}:{}-{}:{})", root.type(),
        locus_start.line() + 1, locus_start.column() + 1,
        locus_end.line() + 1, locus_end.column() + 1);

    class FieldDumper : public NodeFieldReceiver {
    public:
        explicit FieldDumper(
            int indent,
            const char *source_start,
            LineIndexer &line_indexer,
            bool atomic
        )
            : indent(indent)
            , source_start(source_start)
            , line_indexer(line_indexer)
            , atomic(atomic)
        {}

        void
        printFieldName(std::string_view name) {
            if (first) {
                printOutput(":");
                first = false;
            }
            else {
                assert(!atomic);
            }

            if (atomic)
                printOutput(" "); // omit the name, since atomic nodes only have one member
            else
                printOutput("\n{:{}}{} = ", "", indent + INDENT_SIZE, name);
        }

        void
        receiveIdField(std::string_view name, std::string_view value) override {
            printFieldName(name);
            printOutput("{}", value);
        }

        void
        receiveBooleanField(std::string_view name, bool value) override {
            printFieldName(name);
            printOutput("{}", value);
        }

        void
        receiveIntField(std::string_view name, pascal_integer_t value) override {
            printFieldName(name);
            printOutput("{}", value);
        }

        void
        receiveRealField(std::string_view name, pascal_real_t value) override {
            printFieldName(name);
            printOutput("{}", value);
        }

        void
        receiveStringField(std::string_view name, std::string_view value) override {
            printFieldName(name);

            std::string value_with_escapes;
            value_with_escapes.reserve(value.size() + std::ranges::count(value, '\''));
            for (auto c : value) {
                assert(' ' <= c && c <= '~');
                value_with_escapes += c;
                if (c == '\'') value_with_escapes += c;
            }

            printOutput("'{}'", value_with_escapes);
        }

        void
        receiveNodeField(std::string_view name, const Node &value) override {
            printFieldName(name);
            dumpParseTreeHelper(value, indent + INDENT_SIZE, source_start, line_indexer);
        }

        void
        receiveNodeListField(std::string_view name, std::span<const Node *> value) override {
            printFieldName(name);
            printOutput("[");

            for (const auto &p_node : value) {
                printOutput("\n{:{}}", "", indent + INDENT_SIZE * 2);
                dumpParseTreeHelper(*p_node, indent + INDENT_SIZE * 2,
                    source_start, line_indexer);
            }

            if (!value.empty())
                printOutput("\n{:{}}", "", indent + INDENT_SIZE);

            printOutput("]");
        }

        int indent;
        bool first = true;
        const char *source_start;
        LineIndexer &line_indexer;
        bool atomic;
    } dumper(indent, source_start, line_indexer, root.isAtomic());

    root.describeFields(dumper);
}

void
dumpParseTree(const Node &root, const char *source_start, LineIndexer &line_indexer) {
    dumpParseTreeHelper(root, 0, source_start, line_indexer);
    printOutput("\n");
}

int
main(int, char **argv) {
    char **pp_arg = argv + 1;
    bool dump_parse_tree = false;

    for (; *pp_arg; ++pp_arg) {
        if (!std::strcmp(*pp_arg, "--")) {
            ++pp_arg;
            break;
        }

        if (!std::strcmp(*pp_arg, "--dump-parse-tree")) {
            dump_parse_tree = true;
        }
        else if (**pp_arg == '-') {
            printError("unknown option: {}\n", *pp_arg);
            return 1;
        }
        else {
            break;
        }
    }

    if (!pp_arg[0] || pp_arg[1]) {
        printError("usage: {} [--dump-parse-tree] source.pas\n", argv[0]);
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
            reporter.err(&*non_ascii_it, ec::NON_ASCII_CHAR,
                "non-ASCII character (first byte is {:#x})", (unsigned char)*non_ascii_it);
            return 1;
        }
    }

    std::vector<std::unique_ptr<Token>> tokens = lex(source_text, reporter);

    if (reporter.hadErrors())
        return 1;

    auto parse_tree = parse(tokens, reporter);

    if (reporter.hadErrors())
        return 1;

    if (dump_parse_tree) {
        dumpParseTree(parse_tree, source_text.data(), line_indexer);
        return 0;
    }

    auto program = analyze(parse_tree, reporter);

    if (reporter.hadErrors())
        return 1;

    return 0;
}
