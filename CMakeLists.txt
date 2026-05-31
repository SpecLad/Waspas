# SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
#
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 4.2 FATAL_ERROR)

project(Waspas LANGUAGES CXX)
include(CTest)

find_package(Python 3.12 COMPONENTS Interpreter)

set(CMAKE_CXX_STANDARD 23)

if(CMAKE_CXX_COMPILER_ID MATCHES "^GNU$|Clang$")
    add_compile_options(-Wall -Wextra)
elseif(MSVC)
    # If we don't use this option, sizeof(builtin_{function,procedure}_resolve_f)
    # varies in different compilation units depending on whether StatementBuilder
    # is a complete type. The resulting program then crashes at runtime.
    add_compile_options(/vmg)

    add_compile_options(/W3)
endif()

add_custom_command(
    OUTPUT lexing-tables.cxx
    COMMAND Python::Interpreter --
        "${CMAKE_CURRENT_SOURCE_DIR}/generate-lexing-tables.py" > lexing-tables.cxx
    MAIN_DEPENDENCY generate-lexing-tables.py
    VERBATIM
    CODEGEN
)

add_custom_command(
    OUTPUT parsing-nodes.ixx
    COMMAND Python::Interpreter --
        "${CMAKE_CURRENT_SOURCE_DIR}/generate-parsing-nodes.py" > parsing-nodes.ixx
    MAIN_DEPENDENCY generate-parsing-nodes.py
    VERBATIM
    CODEGEN
)

add_executable(waspas
    src/io.cxx
    src/lexing.cxx
    src/main.cxx
    src/parsing.cxx
    src/semantics.cxx
    src/semantics-analyze.cxx
)

target_sources(waspas
    PRIVATE
    FILE_SET CXX_MODULES
    FILES
        src/io.ixx
        src/lexing.ixx
        src/lexing-tokens.ixx
        "${CMAKE_CURRENT_BINARY_DIR}/lexing-tables.cxx"
        src/parsing.ixx
        src/parsing-core.ixx
        "${CMAKE_CURRENT_BINARY_DIR}/parsing-nodes.ixx"
        src/reporting.ixx
        src/semantics.ixx
        src/semantics-builtins.ixx
        src/semantics-constants.ixx
        src/semantics-core.ixx
        src/semantics-expressions.ixx
        src/semantics-statements.ixx
        src/semantics-types.ixx
        src/utilities.ixx
)

if(MSVC AND WIN32)
    target_sources(waspas PRIVATE utf-8.manifest)
endif()

add_test(
    NAME "Python tests"
    COMMAND "${CMAKE_COMMAND}" -E env WASPAS_TEST_EXE_PATH=$<TARGET_FILE:waspas> --
        $<TARGET_FILE:Python::Interpreter> -- "${CMAKE_CURRENT_SOURCE_DIR}/test/run_tests.py"
)
