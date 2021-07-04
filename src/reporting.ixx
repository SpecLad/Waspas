module;

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <vector>

export module reporting;

import io;

export class Locus {
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

export class LineIndexer {
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
    getLocusForOffset(std::size_t offset) const {
        auto it = std::ranges::upper_bound(line_starts_, offset) - 1;

        return Locus(it - line_starts_.begin(), offset - *it);
    }

private:
    std::vector<std::size_t> line_starts_;
};

export class Reporter {
public:
    Reporter(
        const std::filesystem::path &source_path,
        const char *source_start,
        const LineIndexer &line_indexer
    )
        : source_path_str_(source_path.string())
        , source_start_(source_start)
        , line_indexer_(line_indexer)
        , had_errors_(false)
    {}

    bool
    hadErrors() const { return had_errors_; }

    template <typename ...Args>
    void
    err(const char *location, std::string_view error_code,
            std::string_view error_message_format, Args &&...error_message_args) {
        errRaw(location, error_code,
            std::format(error_message_format, std::forward<Args>(error_message_args)...));
    }

private:
    void
    errRaw(const char *location, std::string_view error_code, std::string_view error_message) {
        Locus locus = line_indexer_.getLocusForOffset(location - source_start_);
        printError("{}:{}:{}: error: {} ({})\n",
            source_path_str_, locus.line() + 1, locus.column() + 1,
            error_message, error_code);
        had_errors_ = true;
    }

    std::string source_path_str_;
    const char *source_start_;
    const LineIndexer &line_indexer_;
    bool had_errors_;
};