#ifndef REQUEST_HANDLERS_H
#define REQUEST_HANDLERS_H

#include "httplib.h"
#include "nlohmann/json.hpp"
#include <string>

// Authentication middleware
class AuthMiddleware {
public:
    explicit AuthMiddleware(const std::string& api_key);
    
    // Returns true if authenticated, false otherwise (and sets error response)
    bool authenticate(const httplib::Request& req, httplib::Response& res) const;
    
private:
    std::string api_key_;
};

// Base handler interface
class IRequestHandler {
public:
    virtual ~IRequestHandler() = default;
    virtual void handle(const httplib::Request& req, httplib::Response& res) = 0;
};

// Helper functions for JSON responses
void set_json_response(httplib::Response& res, const nlohmann::json& data, 
                       int status = 200);
void set_error_response(httplib::Response& res, const std::string& message, 
                        int status = 400);
void set_json_error_response(httplib::Response& res, const std::string& error_message,
                             const std::string& error_type, int status,
                             const std::string& details = "");

#endif  // REQUEST_HANDLERS_H
