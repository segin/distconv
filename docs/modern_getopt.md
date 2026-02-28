# Modern C++23 Getopt Library

A header-only, type-safe, and declarative command-line option parser for C++23.

## Location
- Header: `common/include/modern_getopt.hpp`
- Example: `common/examples/modern_getopt_example.cpp`

## Features
- **Type-safe**: Automatic conversion of arguments using `std::from_chars`.
- **Modern**: Leverages `std::expected` for error handling and `std::string_view` for performance.
- **Declarative**: Fluent API for registering flags and options.
- **Help Generation**: Automatic formatting of usage and help messages.

## Usage

### Basic Example

```cpp
#include "modern_getopt.hpp"

int main(int argc, char** argv) {
    modern_getopt::parser p;

    bool verbose = false;
    int count = 0;

    p.add_flag('v', "verbose", "Enable verbose output", &verbose)
     .add_option('c', "count", "Number of iterations", &count);

    auto result = p.parse(argc, argv);
    if (!result) {
        std::cerr << "Error: " << result.error().message << std::endl;
        p.print_help(argv[0]);
        return 1;
    }
    
    // Use verbose and count...
}
```

### Supported Types
The `add_option` method supports any type that `std::from_chars` can handle (integers, floating-point) as well as `std::string`.

### Positional Arguments
Arguments that do not start with `-` or `--` (and are not values for options) are collected as positional arguments.

```cpp
for (const auto& arg : p.positional_arguments()) {
    // Process arg...
}
```

## Error Handling
The `parse` method returns a `std::expected<void, error>`. The `error` struct contains:
- `type`: One of `unknown_option`, `missing_value`, `invalid_value`, or `unexpected_argument`.
- `message`: A descriptive error string.
