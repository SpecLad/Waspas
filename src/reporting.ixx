module;

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <vector>

export module reporting;

import io;

using namespace std::literals;

struct ErrorCode {
    std::string_view str;
};

export
namespace ec {

constexpr ErrorCode AMBIGUOUS_LABEL{"ambiguous-label"sv};
constexpr ErrorCode CIRCULAR_DEFINITION{"circular-definition"sv};
constexpr ErrorCode DISALLOWED_FILE_COMPONENT{"disallowed-file-component"sv};
constexpr ErrorCode DISALLOWED_GOTO_TARGET{"disallowed-goto-target"sv};
constexpr ErrorCode DISALLOWED_PARAMETER_FORM{"disallowed-parameter-form"sv};
constexpr ErrorCode DISALLOWED_PARAMETER_TYPE{"disallowed-parameter-type"sv};
constexpr ErrorCode DISALLOWED_RESULT_TYPE{"disallowed-result-type"sv};
constexpr ErrorCode DUPLICATE_CASE{"duplicate-case"sv};
constexpr ErrorCode DUPLICATE_IDENTIFIER{"duplicate-identifier"sv};
constexpr ErrorCode DUPLICATE_LABEL{"duplicate-label"sv};
constexpr ErrorCode DUPLICATE_PROGRAM_PARAMETER{"duplicate-program-parameter"sv};
constexpr ErrorCode DUPLICATE_SUBROUTINE_DECLARATION{"duplicate-subroutine-declaration"sv};
constexpr ErrorCode INVALID_COMPONENT_ACCESS{"invalid-component-access"sv};
constexpr ErrorCode INVALID_DIRECTIVE{"invalid-directive"sv};
constexpr ErrorCode INVALID_INTEGER{"invalid-integer"sv};
constexpr ErrorCode INVALID_LABEL{"invalid-label"sv};
constexpr ErrorCode INVALID_REAL{"invalid-real"sv};
constexpr ErrorCode INVALID_TOKEN{"invalid-token"sv};
constexpr ErrorCode INVERTED_SUBRANGE_BOUNDS{"inverted-subrange-bounds"sv};
constexpr ErrorCode MISMATCHED_SUBROUTINE_DECLARATION{"mismatched-subroutine-declaration"sv};
constexpr ErrorCode MISSING_CASE{"missing-case"sv};
constexpr ErrorCode MISSING_DELAYED_DECLARATION{"missing-delayed-declaration"sv};
constexpr ErrorCode MISSING_FORWARD_DECLARATION{"missing-forward-declaration"sv};
constexpr ErrorCode MISSING_PROGRAM_PARAMETER_VARIABLE{"missing-program-parameter-variable"sv};
constexpr ErrorCode MISSING_RESULT_ASSIGNMENT{"missing-result-assignment"sv};
constexpr ErrorCode MISSING_SEPARATOR{"missing-separator"sv};
constexpr ErrorCode NON_ARRAY_TYPE{"non-array-type"sv};
constexpr ErrorCode NON_ASCII_CHAR{"non-ascii-char"sv};
constexpr ErrorCode NON_BOOLEAN_TYPE{"non-boolean-type"sv};
constexpr ErrorCode NON_INTEGER_TYPE{"non-integer-type"sv};
constexpr ErrorCode NON_NUMERIC_TYPE{"non-numeric-type"sv};
constexpr ErrorCode NON_ORDINAL_TYPE{"non-ordinal-type"sv};
constexpr ErrorCode NON_RECORD_TYPE{"non-record-type"sv};
constexpr ErrorCode NON_SET_TYPE{"non-set-type"sv};
constexpr ErrorCode OUT_OF_RANGE{"out-of-range"sv};
constexpr ErrorCode PARAMETER_COUNT_MISMATCH{"parameter-count-mismatch"sv};
constexpr ErrorCode THREATENED_CONTROL_VARIABLE{"threatened-control-variable"sv};
constexpr ErrorCode TOO_MANY_ELEMENTS{"too-many-elements"sv};
constexpr ErrorCode TYPE_MISMATCH{"type-mismatch"sv};
constexpr ErrorCode UNDEFINED_IDENTIFIER{"undefined-identifier"sv};
constexpr ErrorCode UNDEFINED_LABEL{"undefined-label"sv};
constexpr ErrorCode UNEXPECTED_TOKEN{"unexpected-token"sv};
constexpr ErrorCode UNUSED_LABEL{"unused-label"sv};
constexpr ErrorCode USE_BEFORE_DEFINITION{"use-before-definition"sv};
constexpr ErrorCode WRONG_IDENTIFIER_KIND{"wrong-identifier-kind"sv};

// should never be used in the final version
constexpr ErrorCode UNSUPPORTED_FEATURE{"unsupported-feature"sv};

}

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
    err(const char *location, ErrorCode error_code,
            std::format_string<Args...> error_message_format, Args &&...error_message_args) {
        errRaw(location, error_code.str,
            std::format(error_message_format, std::forward<Args>(error_message_args)...));
    }

    template <typename ...Args>
    void
    note(const char *location,
            std::format_string<Args...> note_message_format, Args &&...note_message_args) {
        noteRaw(location,
            std::format(note_message_format, std::forward<Args>(note_message_args)...));
    }

    void hold() {
        assert(!holding_);
        holding_ = true;
    }

    void unhold() {
        assert(holding_);

        for (const auto &message : held_messages_) {
            printError("{}", message);
        }

        if (held_had_errors_)
            had_errors_ = true;

        unholdDiscard();
    }

    void unholdDiscard() {
        assert(holding_);

        held_messages_.clear();
        held_had_errors_ = false;
        holding_ = false;
    }

private:
    void
    errRaw(const char *location, std::string_view error_code, std::string_view error_message) {
        Locus locus = line_indexer_.getLocusForOffset(location - source_start_);
        emit("{}:{}:{}: error: {} ({})\n",
            source_path_str_, locus.line() + 1, locus.column() + 1,
            error_message, error_code);

        if (holding_)
            held_had_errors_ = true;
        else
            had_errors_ = true;
    }

    void
    noteRaw(const char *location, std::string_view note_message) {
        Locus locus = line_indexer_.getLocusForOffset(location - source_start_);
        emit("{}:{}:{}: note: {}\n",
            source_path_str_, locus.line() + 1, locus.column() + 1,
            note_message);
    }

    template <typename ...Args>
    void
    emit(std::format_string<Args...> format, Args &&...args) {
        if (holding_)
            held_messages_.push_back(std::format(format, std::forward<Args>(args)...));
        else
            printError(format, std::forward<Args>(args)...);
    }

    std::string source_path_str_;
    const char *source_start_;
    const LineIndexer &line_indexer_;
    bool had_errors_;

    bool holding_ = false;
    std::vector<std::string> held_messages_;
    bool held_had_errors_ = false;
};
