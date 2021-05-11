module;

#include <fmt/core.h>

#include <cstdio>
#include <string_view>
#include <utility>

export module io;

#if _WIN32

void
print_console(std::FILE *file, std::string_view message);

bool
is_console(std::FILE *file);

#endif

export
template <typename ...Args>
void
print_error(std::string_view format, Args &&...args) {
#if _WIN32
    if (is_console(stderr)) {
        print_console(
            stderr,
            fmt::format(format, std::forward<Args>(args)...));
        return;
    }
#endif

    fmt::print(stderr, format, std::forward<Args>(args)...);
}
