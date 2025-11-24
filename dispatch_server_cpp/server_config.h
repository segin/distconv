#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>
#include <vector>

namespace distconv {
namespace DispatchServer {

struct ServerConfig {
    std::string api_key = "";
    std::string database_path = "dispatch_server.db";
    int port = 8080;
    bool show_help = false;
    bool parse_error = false;
    std::string error_message = "";
};

ServerConfig parse_arguments(int argc, char* argv[]);

} // namespace DispatchServer
} // namespace distconv

#endif // SERVER_CONFIG_H
