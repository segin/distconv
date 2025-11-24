#include "server_config.h"
#include <iostream>
#include <stdexcept>

ServerConfig parse_arguments(int argc, char* argv[]) {
    ServerConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--api-key" && i + 1 < argc) {
            config.api_key = argv[++i];
        } else if (arg == "--database" && i + 1 < argc) {
            config.database_path = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                config.port = std::stoi(argv[++i]);
            } catch (const std::exception& e) {
                config.parse_error = true;
                config.error_message = "Invalid port number: " + std::string(argv[i]);
                return config;
            }
        } else if (arg == "--help") {
            config.show_help = true;
            return config;
        }
    }
    
    return config;
}
