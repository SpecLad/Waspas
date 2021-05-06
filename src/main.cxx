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
            std::cerr << "unknown option: " << *pp_arg << "\n";
            return 1;
        }
    }

    if (!pp_arg[0] || pp_arg[1]) {
        std::cerr << "usage: " << argv[0] << " source.pas\n";
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
            std::cerr << source_path.string() << ": unable to open file (" << e.code().message() << ")\n";
            return 1;
        }

        try {
            source_text.assign(
                std::istreambuf_iterator<char>(source_file),
                std::istreambuf_iterator<char>());
        }
        catch (std::system_error& e) {
            std::cerr << source_path.string() << ": unable to read file (" << e.code().message() << ")\n";
            return 1;
        }
    }

    return 0;
}
