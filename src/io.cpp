module;

#if _WIN32
#define NOMINMAX
#include <windows.h>

#include <io.h>

#include <cassert>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>
#endif

module io;

#if _WIN32

bool
is_console(std::FILE* file) {
    // _isatty returns a true value when the file descriptor is a "character
    // device", which is not necessarily a console. So instead, we test for
    // console-ness by checking whether GetConsoleMode succeeds, which is the
    // method recommended in <https://docs.microsoft.com/en-us/windows/console/writeconsole>.

    HANDLE handle = HANDLE(_get_osfhandle(_fileno(file)));

    DWORD mode;
    return GetConsoleMode(handle, &mode) != FALSE;
}

namespace {

void
print_console_fail() {
    throw std::runtime_error("failed to print to console");
}

}

void
print_console(std::FILE* file, std::string_view message) {
    if (message.size() == 0) return;

    assert(message.size() <= (unsigned)std::numeric_limits<int>::max());

    int message_wide_len = MultiByteToWideChar(
        CP_ACP, 0, message.data(), int(message.size()),
        nullptr, 0);

    if (message_wide_len <= 0) print_console_fail();

    std::vector<wchar_t> message_wide(message_wide_len);

    message_wide_len = MultiByteToWideChar(
        CP_ACP, 0, message.data(), int(message.size()),
        message_wide.data(), message_wide_len);

    if (message_wide_len <= 0) print_console_fail();

    HANDLE handle = HANDLE(_get_osfhandle(_fileno(file)));
    DWORD num_written;

    BOOL success = WriteConsoleW(
        handle, message_wide.data(), message_wide_len, &num_written, nullptr);

    if (!success || num_written != (DWORD)message_wide_len)
        print_console_fail();
}

#endif
