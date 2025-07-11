#include "response_utils.h"
#include <regex>

namespace ResponseUtils {
    
    void success_response(httplib::Response& res, const nlohmann::json& data, int status_code) {
        res.status = status_code;
        res.set_content(data.dump(), "application/json");
    }
    
    void error_response(httplib::Response& res, const std::string& error_code, const std::string& message, int status_code) {
        nlohmann::json error_obj;
        error_obj["error"] = {
            {"code", error_code},
            {"message", message}
        };
        res.status = status_code;
        res.set_content(error_obj.dump(), "application/json");
    }
    
    void json_parse_error(httplib::Response& res, const std::exception& e) {
        error_response(res, "JSON_PARSE_ERROR", "Invalid JSON: " + std::string(e.what()), 400);
    }
    
    void validation_error(httplib::Response& res, const std::string& field, const std::string& issue) {
        error_response(res, "VALIDATION_ERROR", "Field '" + field + "': " + issue, 400);
    }
    
    void not_found_error(httplib::Response& res, const std::string& resource_type, const std::string& id) {
        error_response(res, "NOT_FOUND", resource_type + " with ID '" + id + "' not found", 404);
    }
    
    void unauthorized_error(httplib::Response& res, const std::string& reason) {
        std::string message = "Unauthorized access";
        if (!reason.empty()) {
            message += ": " + reason;
        }
        error_response(res, "UNAUTHORIZED", message, 401);
    }
    
    void server_error(httplib::Response& res, const std::string& message) {
        error_response(res, "INTERNAL_ERROR", message, 500);
    }
    
    bool validate_json_content_type(const httplib::Request& req, httplib::Response& res) {
        std::string content_type = req.get_header_value("Content-Type");
        if (content_type.empty() || content_type.find("application/json") == std::string::npos) {
            error_response(res, "INVALID_CONTENT_TYPE", 
                         "Content-Type must be 'application/json'", 400);
            return false;
        }
        return true;
    }
    
    bool is_valid_url(const std::string& url) {
        if (url.empty()) return false;
        
        // Basic URL validation regex
        std::regex url_regex(R"(^https?://[^\s/$.?#].[^\s]*$)", std::regex_constants::icase);
        return std::regex_match(url, url_regex);
    }
    
    bool validate_output_url(httplib::Response& res, const std::string& url) {
        if (!is_valid_url(url)) {
            validation_error(res, "output_url", "Must be a valid HTTP or HTTPS URL");
            return false;
        }
        return true;
    }
    
    bool is_valid_job_state_transition(const std::string& current_state, const std::string& new_state) {
        // Define valid state transitions
        if (current_state == "pending") {
            return new_state == "assigned" || new_state == "failed" || new_state == "cancelled";
        } else if (current_state == "assigned") {
            return new_state == "completed" || new_state == "failed" || new_state == "failed_retry";
        } else if (current_state == "failed_retry") {
            return new_state == "pending" || new_state == "failed_permanently";
        } else if (current_state == "failed") {
            return new_state == "pending" || new_state == "failed_permanently";
        }
        // Terminal states
        return false;
    }
}