module;

#include <memory>
#include <optional>
#include <span>

export module parsing;

import lexing;
import reporting;

struct NodeProgram {};

export
std::optional<NodeProgram>
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
);