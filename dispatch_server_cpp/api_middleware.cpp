#include "api_middleware.h"
#include <iostream>

void ApiMiddleware::validate_api_key(const httplib::Request& req, httplib::Response& res, 
                                   const std::string& api_key, Handler next_handler) {
    // Skip validation if no API key is configured
    if (api_key.empty()) {
        next_handler(req, res);
        return;
    }
    
    if (!is_api_key_valid(req, api_key)) {
        if (req.get_header_value("X-API-Key").empty()) {
            set_unauthorized_response(res, "Unauthorized: Missing 'X-API-Key' header.");
        } else {
            set_unauthorized_response(res, "Unauthorized");
        }
        return;
    }
    
    // API key is valid, proceed with the actual handler
    next_handler(req, res);
}

ApiMiddleware::Handler ApiMiddleware::with_api_key_validation(const std::string& api_key, Handler handler) {
    return [api_key, handler](const httplib::Request& req, httplib::Response& res) {
        validate_api_key(req, res, api_key, handler);
    };
}

void ApiMiddleware::set_unauthorized_response(httplib::Response& res, const std::string& message) {
    res.status = 401;
    res.set_content(message, "text/plain");
}

bool ApiMiddleware::is_api_key_valid(const httplib::Request& req, const std::string& api_key) {
    std::string provided_key = req.get_header_value("X-API-Key");
    return provided_key == api_key;
}