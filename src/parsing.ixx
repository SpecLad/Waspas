module;

#include <memory>
#include <span>

export module parsing;

import lexing;
import reporting;

struct NodeProgram {};

export
NodeProgram
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
);