#pragma once

#include <algorithm>
#include <expected>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>

namespace modern_getopt {

enum class error_type {
    unknown_option,
    missing_value,
    invalid_value,
    unexpected_argument
};

struct error {
    error_type type;
    std::string message;
};

class parser {
public:
    struct option {
        char short_name = '\0';
        std::string long_name;
        std::string description;
        bool required = false;
        bool has_value = false;
        std::function<std::expected<void, std::string>(std::string_view)> action;
    };

    parser& add_option(char short_name, std::string_view long_name, std::string_view description, auto* target) {
        options_.push_back({
            .short_name = short_name,
            .long_name = std::string(long_name),
            .description = std::string(description),
            .has_value = true,
            .action = [target](std::string_view val) -> std::expected<void, std::string> {
                if constexpr (std::is_same_v<decltype(target), bool*>) {
                    *target = (val == "true" || val == "1" || val == "yes");
                    return {};
                } else if constexpr (std::is_integral_v<std::remove_pointer_t<decltype(target)>>) {
                    auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), *target);
                    if (ec != std::errc{}) return std::unexpected("Invalid integer value: " + std::string(val));
                    return {};
                } else if constexpr (std::is_floating_point_v<std::remove_pointer_t<decltype(target)>>) {
                    auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), *target);
                    if (ec != std::errc{}) return std::unexpected("Invalid floating point value: " + std::string(val));
                    return {};
                } else if constexpr (std::is_same_v<std::remove_pointer_t<decltype(target)>, std::string>) {
                    *target = std::string(val);
                    return {};
                }
                return std::unexpected("Unsupported type for option");
            }
        });
        return *this;
    }

    parser& add_flag(char short_name, std::string_view long_name, std::string_view description, bool* target) {
        options_.push_back({
            .short_name = short_name,
            .long_name = std::string(long_name),
            .description = std::string(description),
            .has_value = false,
            .action = [target](std::string_view) -> std::expected<void, std::string> {
                *target = true;
                return {};
            }
        });
        return *this;
    }

    std::expected<void, error> parse(int argc, char** argv) {
        std::vector<std::string_view> args(argv + 1, argv + argc);
        
        for (size_t i = 0; i < args.size(); ++i) {
            std::string_view arg = args[i];
            
            if (arg.starts_with("--")) {
                auto long_name = arg.substr(2);
                auto it = std::ranges::find_if(options_, [&](const option& opt) { return opt.long_name == long_name; });
                
                if (it == options_.end()) {
                    return std::unexpected(error{error_type::unknown_option, "Unknown option: " + std::string(arg)});
                }

                if (it->has_value) {
                    if (i + 1 >= args.size()) {
                        return std::unexpected(error{error_type::missing_value, "Option " + std::string(arg) + " requires a value"});
                    }
                    auto result = it->action(args[++i]);
                    if (!result) return std::unexpected(error{error_type::invalid_value, result.error()});
                } else {
                    it->action("");
                }
            } else if (arg.starts_with("-") && arg.size() > 1) {
                for (size_t j = 1; j < arg.size(); ++j) {
                    char short_name = arg[j];
                    auto it = std::ranges::find_if(options_, [&](const option& opt) { return opt.short_name == short_name; });
                    
                    if (it == options_.end()) {
                        return std::unexpected(error{error_type::unknown_option, "Unknown short option: -" + std::string(1, short_name)});
                    }

                    if (it->has_value) {
                        if (j + 1 < arg.size()) {
                            // Value is attached to the short option like -oValue
                            auto result = it->action(arg.substr(j + 1));
                            if (!result) return std::unexpected(error{error_type::invalid_value, result.error()});
                            break; // Done with this arg
                        } else {
                            // Value is the next argument
                            if (i + 1 >= args.size()) {
                                return std::unexpected(error{error_type::missing_value, "Option -" + std::string(1, short_name) + " requires a value"});
                            }
                            auto result = it->action(args[++i]);
                            if (!result) return std::unexpected(error{error_type::invalid_value, result.error()});
                        }
                    } else {
                        it->action("");
                    }
                }
            } else {
                positional_arguments_.push_back(std::string(arg));
            }
        }
        return {};
    }

    void print_help(std::string_view program_name) const {
        std::cout << std::format("Usage: {} [options]", program_name) << std::endl;
        std::cout << "\nOptions:" << std::endl;
        for (const auto& opt : options_) {
            std::string short_str = opt.short_name ? std::string("-") + opt.short_name + ", " : "    ";
            std::cout << std::format("  {: <4} --{: <15} {}", short_str, opt.long_name, opt.description) << std::endl;
        }
    }

    const std::vector<std::string>& positional_arguments() const {
        return positional_arguments_;
    }

private:
    std::vector<option> options_;
    std::vector<std::string> positional_arguments_;
};

} // namespace modern_getopt
