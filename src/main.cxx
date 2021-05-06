#include <fmt/format.h>
#include <fmt/ostream.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

int
main(int, char **argv) {
    char **pp_arg = argv + 1;

    if (*pp_arg) {
        if (!std::strcmp(*pp_arg, "--")) {
            ++pp_arg;
        }
        else if (**pp_arg == '-') {
            fmt::print(std::cerr, "unknown option: {}\n", *pp_arg);
            return 1;
        }
    }

    if (!pp_arg[0] || pp_arg[1]) {
        fmt::print(std::cerr, "usage: {} source.pas\n", argv[0]);
        return 1;
    }

    std::filesystem::path source_path(pp_arg[0]);
    std::string source_text;

    {
        std::ifstream source_file;
        source_file.exceptions(std::ios::failbit | std::ios::badbit);

        try {
            source_file.open(source_path, std::ios::binary);
        }
        catch (std::system_error& e) {
            fmt::print(std::cerr, "{}: unable to open file ({})\n",
                source_path.string(), e.code().message());
            return 1;
        }

        try {
            source_text.assign(
                std::istreambuf_iterator<char>(source_file),
                std::istreambuf_iterator<char>());
        }
        catch (std::system_error& e) {
            fmt::print(std::cerr, "{}: unable to read file ({})\n",
                source_path.string(), e.code().message());
            return 1;
        }
    }

    return 0;
}
