#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <typeinfo>

import io;
import lexer;
import reporting;

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

    Reporter reporter(source_path);
    LineIndexer line_indexer(source_text);

    {
        auto non_ascii_it = std::find_if(source_text.begin(), source_text.end(),
            [](char c) { return (unsigned char)c >= 0x80; });

        if (non_ascii_it != source_text.end()) {
            Locus locus = line_indexer.getLocusForOffset(non_ascii_it - source_text.begin());
            reporter.err(locus, "non-ascii-char",
                "non-ASCII character (first byte is {:#x})", (unsigned char)*non_ascii_it);
            return 1;
        }
    }

    std::vector<std::unique_ptr<Token>> tokens = lex(source_text);

    for (const auto &p_token : tokens) {
        std::string_view token_view = p_token->view();
        Locus locus_start = line_indexer.getLocusForOffset(
            token_view.data() - source_text.data());
        Locus locus_end = line_indexer.getLocusForOffset(
            token_view.data() + token_view.size() - source_text.data());

        print_error("{}:{}:{}-{}:{}: {} ({})\n", source_path.string(),
            locus_start.line() + 1, locus_start.column() + 1,
            locus_end.line() + 1, locus_end.column() + 1,
            token_view, typeid(*p_token).name());
    }

    return 0;
}
