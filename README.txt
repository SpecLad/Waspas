Waspas
======

Introduction
------------

Waspas is a compiler from Standard Pascal (as defined by ISO 7185:1990)
to WebAssembly. It uses the WebAssembly System Interface (WASI) to implement
I/O functionality.

... or at least, that's what it's supposed to be eventually. Currently,
the frontend is feature-complete, and the backend is nonexistent. You can dump
the abstract syntax tree, but you cannot actually compile anything.

The name "Waspas" is an abbreviation for "WebAssembly Standard Pascal".

Status
------

Waspas is purely a toy project, created to experiment and to have fun.
There is no guarantee of continued development or maintenance.
There is also no guarantee of backwards compatibility if development
does happen.

In other words, do not depend on this for anything serious or you will
be fired.

Licensing
---------

Waspas is free software, available under the terms of the Mozilla Public
License, Version 2.0.

Building
--------

To build Waspas you will need a C++ compiler, CMake and Python.

You can find the minimal versions of the latter two dependencies in
CMakeLists.txt. As for the compiler, I don't know the exact minimum,
but as of June 2026, Waspas has been known to build with GCC 15 and MSVC 19.

You will also need a low-level build system for CMake to use. Note that
since this project uses C++ modules, only a few build systems are supported.
See <https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html#generator-support>
for more details.

The build and test commands are standard for a CMake-based project.
Let <GENERATOR> be your chosen generator from the list on the page linked
above, and <CONFIG> the desired build configuration (Debug or Release).
If <GENERATOR> is multi-config, run the following:

    cmake -B build -G "<GENERATOR>"
    cmake --build build --config "<CONFIG>"
    ctest --test-dir build --build-config "<CONFIG>"

Otherwise, run the following:

    cmake -B build -G "<GENERATOR>" -D "CMAKE_BUILD_TYPE=<CONFIG>"
    cmake --build build
    ctest --test-dir build

Usage
-----

To compile program.pas, run:

    waspas program.pas

Currently, code generation is not implemented, so this will just check
the syntax and exit.

To print a textual representation of the abstract syntax tree of
program.pas, run:

    waspas --dump-ast program.pas
