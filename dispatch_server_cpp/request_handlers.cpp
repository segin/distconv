#include "request_handlers.h"
#include <iostream>

// AuthMiddleware implementation
AuthMiddleware::AuthMiddleware(const std::string& api_key) 
    : api_key_(api_key) {}

bool AuthMiddleware::authenticate(const httplib::Request& req, 
                                   httplib::Response& res) const {
    // If no API key is configured, allow all requests
    if (api_key_.empty()) {
        return true;
    }
    
    std::string provided_key = req.get_header_value("X-API-Key");
    
    // Check for missing header
    if (provided_key.empty()) {
        res.status = 401;
        res.set_content("Unauthorized: Missing 'X-API-Key' header.", "text/plain");
        return false;
    }
    
    // Check for incorrect key
    if (provided_key != api_key_) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
        return false;
    }
    
    return true;
}

// Helper functions
void set_json_response(httplib::Response& res, const nlohmann::json& data, int status) {
    res.status = status;
    res.set_content(data.dump(), "application/json");
}

void set_error_response(httplib::Response& res, const std::string& message, int status) {
    res.status = status;
    res.set_content(message, "text/plain");
}

void set_json_error_response(httplib::Response& res, const std::string& error_message, 
                             const std::string& error_type, int status,
                             const std::string& details) {
    res.status = status;
    nlohmann::json error_obj;
    error_obj["error"] = error_message;
    error_obj["error_type"] = error_type;
    if (!details.empty()) {
        error_obj["details"] = details;
    }
    error_obj["status"] = status;
    res.set_content(error_obj.dump(), "application/json");
}
