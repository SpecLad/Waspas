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

template <typename ...Args>
void
printMessage(std::ostream &stream, std::FILE *file, std::format_string<Args...> format, Args &&...args) {
    std::string message = std::format(format, std::forward<Args>(args)...);

#if _WIN32
    if (isConsole(file)) {
        printConsole(file, message);
        return;
    }
#else
    (void)file;
#endif

    stream << message;
}

export
template <typename ...Args>
void
printError(std::format_string<Args...> format, Args &&...args) {
    printMessage(std::cerr, stderr, format, std::forward<Args>(args)...);
}

export
template <typename ...Args>
void
printOutput(std::format_string<Args...> format, Args &&...args) {
    printMessage(std::cout, stdout, format, std::forward<Args>(args)...);
}
