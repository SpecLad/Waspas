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
dumpAstHelper(const Node &root, int indent) {
    static constexpr int INDENT_SIZE = 4;

    print_error("{:{}}{}", "", indent, root.type());

    class FieldDumper : public NodeFieldReceiver {
    public:
        explicit FieldDumper(int indent) : indent(indent) {}

        void
        printFieldName(std::string_view name) {
            if (first) {
                print_error(":");
                first = false;
            }

            print_error("\n{:{}}{} = ", "", indent + INDENT_SIZE, name);
        }

        void
        receiveIdField(std::string_view name, std::string_view value) override {
            printFieldName(name);
            print_error("{}", value);
        }

        void
        receiveNodeListField(std::string_view name, std::span<const Node *> value) {
            printFieldName(name);
            print_error("[");

            for (const auto &p_node : value) {
                print_error("\n");
                dumpAstHelper(*p_node, indent + INDENT_SIZE * 2);
            }

            if (!value.empty())
                print_error("\n{:{}}", "", indent + INDENT_SIZE);

            print_error("]");
        }

        int indent;
        bool first = true;
    } dumper(indent);

    root.describeFields(dumper);
}

void
dumpAst(const Node &root) {
    dumpAstHelper(root, 0);
    print_error("\n");
}

int
main(int, char **argv) {
    char **pp_arg = argv + 1;

    if (*pp_arg) {
        if (!std::strcmp(*pp_arg, "--")) {
            ++pp_arg;
        }
        else if (**pp_arg == '-') {
            print_error("unknown option: {}\n", *pp_arg);
            return 1;
        }
    }

    if (!pp_arg[0] || pp_arg[1]) {
        print_error("usage: {} source.pas\n", argv[0]);
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
            print_error("{}: unable to open file ({})\n",
                source_path.string(), e.code().message());
            return 1;
        }

        try {
            source_text.assign(
                std::istreambuf_iterator<char>(source_file),
                std::istreambuf_iterator<char>());
        }
        catch (std::system_error& e) {
            print_error("{}: unable to read file ({})\n",
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

    dumpAst(ast);

    return 0;
}
