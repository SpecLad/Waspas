// SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
//
// SPDX-License-Identifier: MPL-2.0

module;

#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

export module parsingng;

import lexing;
import reporting;

struct Node {
    std::string name;

    std::string_view view;

    std::vector<std::unique_ptr<Node>> subnodes;
};

export
std::unique_ptr<Node>
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
);
