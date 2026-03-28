module;

#include <cstdio>
#include <format>
#include <iostream>
#include <string_view>
#include <utility>

export module io;

#if _WIN32

void
printConsole(std::FILE *file, std::string_view message);

bool
isConsole(std::FILE *file);

#endif

export
template <typename ...Args>
void
printError(std::string_view format, Args &&...args) {
    std::string message = std::vformat(format,
        std::make_format_args(args...));

#if _WIN32
    if (isConsole(stderr)) {
        printConsole(stderr, message);
        return;
    }
#endif

    std::cerr << message;
}
