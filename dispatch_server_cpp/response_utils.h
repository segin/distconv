#ifndef RESPONSE_UTILS_H
#define RESPONSE_UTILS_H

#include "httplib.h"
#include "nlohmann/json.hpp"
#include <string>

namespace distconv {
namespace DispatchServer {
namespace ResponseUtils {
    // Standard response creation functions
    void success_response(httplib::Response& res, const nlohmann::json& data, int status_code = 200);
    void error_response(httplib::Response& res, const std::string& error_code, const std::string& message, int status_code = 400);
    void json_parse_error(httplib::Response& res, const std::exception& e);
    void validation_error(httplib::Response& res, const std::string& field, const std::string& issue);
    void not_found_error(httplib::Response& res, const std::string& resource_type, const std::string& id);
    void unauthorized_error(httplib::Response& res, const std::string& reason = "");
    void server_error(httplib::Response& res, const std::string& message);
    
    // Content-Type validation
    bool validate_json_content_type(const httplib::Request& req, httplib::Response& res);
    
    // URL validation
    bool is_valid_url(const std::string& url);
    bool validate_output_url(httplib::Response& res, const std::string& url);
    
    // Job state validation
    bool is_valid_job_state_transition(const std::string& current_state, const std::string& new_state);
}
} // namespace DispatchServer
} // namespace distconv

#endif // RESPONSE_UTILS_H