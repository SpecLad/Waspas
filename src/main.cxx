#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

import io;
import lexer;

class Locus {
public:
    Locus(std::size_t line, std::size_t column)
        : line_(line)
        , column_(column)
    {}

    std::size_t
    line() const { return line_; }
    std::size_t
    column() const { return column_; }

private:
    std::size_t line_;
    std::size_t column_;
};

class LineIndexer {
public:
    LineIndexer(std::string_view source) {
        std::size_t line_start = 0;
        line_starts_.push_back(line_start);

        for (;;) {
            std::size_t newline_index = source.find('\n', line_start);
            if (newline_index == std::string_view::npos) return;

            line_start = newline_index + 1;
            line_starts_.push_back(line_start);
        }
    }

    Locus
    getLocusForOffset(std::size_t offset) {
        auto it = std::ranges::upper_bound(line_starts_, offset) - 1;

        return Locus(it - line_starts_.begin(), offset - *it);
    }

private:
    std::vector<std::size_t> line_starts_;
};

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

    {
        auto non_ascii_it = std::find_if(source_text.begin(), source_text.end(),
            [](char c) { return (unsigned char)c >= 0x80; });

        if (non_ascii_it != source_text.end()) {
            Locus locus = line_indexer.getLocusForOffset(non_ascii_it - source_text.begin());
            print_error("{}:{}:{}: non-ASCII character\n",
                source_path.string(), locus.line() + 1, locus.column() + 1);
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

        print_error("{}:{}:{}-{}:{}: {}\n", source_path.string(),
            locus_start.line() + 1, locus_start.column() + 1,
            locus_end.line() + 1, locus_end.column() + 1,
            token_view);
    }

    return 0;
}
