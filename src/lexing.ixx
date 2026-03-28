// SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
//
// SPDX-License-Identifier: MPL-2.0

module;

#include <cassert>

export module lexing;

export import :tokens;

import std;

import reporting;

export
std::vector<std::unique_ptr<Token>>
lex(std::string_view source, Reporter &reporter);
