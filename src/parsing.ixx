module;

#include <memory>
#include <span>
#include <string>

export module parsing;

import lexing;
import reporting;

export
class NodeProgram {
public:
    std::string name;
};

export
NodeProgram
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
);
