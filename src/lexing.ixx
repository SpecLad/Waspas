module;

#include <memory>
#include <vector>

export module lexing;

export import :tokens;

import reporting;

export
std::vector<std::unique_ptr<Token>>
lex(std::string_view source, Reporter &reporter);
