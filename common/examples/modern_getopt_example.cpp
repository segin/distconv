#include "../include/modern_getopt.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    modern_getopt::parser p;

    bool verbose = false;
    int count = 0;
    std::string name = "World";
    double threshold = 0.5;

    p.add_flag('v', "verbose", "Enable verbose output", &verbose)
     .add_option('c', "count", "Number of iterations", &count)
     .add_option('n', "name", "Name to greet", &name)
     .add_option('t', "threshold", "Success threshold", &threshold);

    auto result = p.parse(argc, argv);
    if (!result) {
        std::cerr << std::format("Error: {}", result.error().message) << std::endl;
        p.print_help(argv[0]);
        return 1;
    }

    if (verbose) {
        std::cout << "Verbose mode enabled" << std::endl;
    }

    std::cout << std::format("Hello, {}!", name) << std::endl;
    std::cout << std::format("Count: {}", count) << std::endl;
    std::cout << std::format("Threshold: {}", threshold) << std::endl;

    if (!p.positional_arguments().empty()) {
        std::cout << "\nPositional arguments:" << std::endl;
        for (const auto& arg : p.positional_arguments()) {
            std::cout << std::format("  {}", arg) << std::endl;
        }
    }

    return 0;
}
