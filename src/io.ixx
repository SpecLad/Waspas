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
printError(std::format_string<Args...> format, Args &&...args) {
    std::string message = std::format(format, std::forward<Args>(args)...);

#if _WIN32
    if (isConsole(stderr)) {
        printConsole(stderr, message);
        return;
    }
#endif

    std::cerr << message;
}
