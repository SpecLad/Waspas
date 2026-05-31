// SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
//
// SPDX-License-Identifier: MPL-2.0

module;

#include <memory>
#include <span>

export module parsing;

export import :nodes;

import lexing;
import reporting;

export
nodes::Program
parse(
    std::span<const std::unique_ptr<Token>> tokens,
    Reporter &reporter
);
