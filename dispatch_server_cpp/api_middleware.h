#ifndef API_MIDDLEWARE_H
#define API_MIDDLEWARE_H

#include "httplib.h"
#include <string>
#include <functional>

namespace distconv {
namespace DispatchServer {

class ApiMiddleware {
public:
    // Type alias for endpoint handlers
    using Handler = std::function<void(const httplib::Request&, httplib::Response&)>;
    
    // Validates API key and calls the next handler if valid
    static void validate_api_key(const httplib::Request& req, httplib::Response& res, 
                                const std::string& api_key, Handler next_handler);
    
    // Creates a wrapped handler with API key validation
    static Handler with_api_key_validation(const std::string& api_key, Handler handler);
    
    // Helper function to set standard error responses
    static void set_unauthorized_response(httplib::Response& res, const std::string& message);
    
private:
    // Check if API key is valid
    static bool is_api_key_valid(const httplib::Request& req, const std::string& api_key);
};

} // namespace DispatchServer
} // namespace distconv

#endif // API_MIDDLEWARE_H