#include <cstring>
#include <filesystem>
#include <iostream>

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

    std::cout << "source path: " << source_path << std::endl;

    return 0;
}
