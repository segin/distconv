#include "dispatch_server_core.h"
#include <regex>

// Enhanced endpoint implementations with improved validation and features

namespace EnhancedEndpoints {
    
    // Utility functions for validation
    bool is_valid_url(const std::string& url) {
        if (url.empty()) return false;
        std::regex url_regex(R"(^https?://[^\s/$.?#].[^\s]*$)", std::regex_constants::icase);
        return std::regex_match(url, url_regex);
    }
    
    bool validate_json_content_type(const httplib::Request& req, httplib::Response& res) {
        std::string content_type = req.get_header_value("Content-Type");
        if (content_type.empty() || content_type.find("application/json") == std::string::npos) {
            nlohmann::json error;
            error["error"] = {{"code", "INVALID_CONTENT_TYPE"}, {"message", "Content-Type must be 'application/json'"}};
            res.status = 400;
            res.set_content(error.dump(), "application/json");
            return false;
        }
        return true;
    }
    
    void error_response(httplib::Response& res, const std::string& code, const std::string& message, int status = 400) {
        nlohmann::json error;
        error["error"] = {{"code", code}, {"message", message}};
        res.status = status;
        res.set_content(error.dump(), "application/json");
    }
    
    void success_response(httplib::Response& res, const nlohmann::json& data, int status = 200) {
        res.status = status;
        res.set_content(data.dump(), "application/json");
    }
    
    // API Key validation middleware
    bool validate_api_key(const httplib::Request& req, httplib::Response& res, const std::string& api_key) {
        if (api_key.empty()) return true; // No auth required
        
        std::string provided_key = req.get_header_value("X-API-Key");
        if (provided_key.empty()) {
            error_response(res, "UNAUTHORIZED", "Missing 'X-API-Key' header", 401);
            return false;
        }
        if (provided_key != api_key) {
            error_response(res, "UNAUTHORIZED", "Invalid API key", 401);
            return false;
        }
        return true;
    }
}

// Enhanced job submission with better validation and error handling
void setup_enhanced_job_endpoints(httplib::Server &svr, const std::string& api_key) {
    
    // Enhanced job submission endpoint
    svr.Post("/api/v1/jobs", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        if (!EnhancedEndpoints::validate_json_content_type(req, res)) return;
        
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            
            // Enhanced validation with specific error messages
            if (!request_json.contains("source_url") || !request_json["source_url"].is_string()) {
                EnhancedEndpoints::error_response(res, "VALIDATION_ERROR", "Field 'source_url' is required and must be a string");
                return;
            }
            
            if (!request_json.contains("target_codec") || !request_json["target_codec"].is_string()) {
                EnhancedEndpoints::error_response(res, "VALIDATION_ERROR", "Field 'target_codec' is required and must be a string");
                return;
            }
            
            // Validate source URL format
            std::string source_url = request_json["source_url"];
            if (!EnhancedEndpoints::is_valid_url(source_url)) {
                EnhancedEndpoints::error_response(res, "VALIDATION_ERROR", "Field 'source_url' must be a valid HTTP or HTTPS URL");
                return;
            }
            
            // Validate optional fields
            if (request_json.contains("job_size") && !request_json["job_size"].is_number()) {
                EnhancedEndpoints::error_response(res, "VALIDATION_ERROR", "Field 'job_size' must be a number");
                return;
            }
            
            if (request_json.contains("max_retries") && !request_json["max_retries"].is_number_integer()) {
                EnhancedEndpoints::error_response(res, "VALIDATION_ERROR", "Field 'max_retries' must be an integer");
                return;
            }
            
            if (request_json.contains("priority") && (!request_json["priority"].is_number_integer() || 
                request_json["priority"].get<int>() < 0 || request_json["priority"].get<int>() > 2)) {
                EnhancedEndpoints::error_response(res, "VALIDATION_ERROR", "Field 'priority' must be an integer between 0 and 2");
                return;
            }
            
            // Create job with enhanced data
            std::string job_id = generate_uuid();
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            
            nlohmann::json job;
            job["job_id"] = job_id;
            job["source_url"] = source_url;
            job["target_codec"] = request_json["target_codec"];
            job["job_size"] = request_json.value("job_size", 0.0);
            job["status"] = "pending";
            job["assigned_engine"] = nullptr;
            job["output_url"] = nullptr;
            job["retries"] = 0;
            job["max_retries"] = request_json.value("max_retries", 3);
            job["priority"] = request_json.value("priority", 0);
            job["created_at"] = now_ms;
            job["updated_at"] = now_ms;
            job["retry_state"] = "none"; // none, retry_scheduled, failed_retry
            
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                jobs_db[job_id] = job;
                save_state_unlocked();
            }
            
            EnhancedEndpoints::success_response(res, job, 201);
            
        } catch (const nlohmann::json::parse_error& e) {
            EnhancedEndpoints::error_response(res, "JSON_PARSE_ERROR", "Invalid JSON: " + std::string(e.what()));
        } catch (const std::exception& e) {
            EnhancedEndpoints::error_response(res, "INTERNAL_ERROR", "Server error: " + std::string(e.what()), 500);
        }
    });
    
    // Enhanced job status endpoint with better error handling
    svr.Get(R"(/api/v1/jobs/([a-fA-F0-9\-]{36}))", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        std::string job_id = req.matches[1];
        
        std::lock_guard<std::mutex> lock(state_mutex);
        if (!jobs_db.contains(job_id)) {
            EnhancedEndpoints::error_response(res, "NOT_FOUND", "Job with ID '" + job_id + "' not found", 404);
            return;
        }
        
        EnhancedEndpoints::success_response(res, jobs_db[job_id]);
    });
    
    // Job cancellation endpoint
    svr.Delete(R"(/api/v1/jobs/([a-fA-F0-9\-]{36}))", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        std::string job_id = req.matches[1];
        
        std::lock_guard<std::mutex> lock(state_mutex);
        if (!jobs_db.contains(job_id)) {
            EnhancedEndpoints::error_response(res, "NOT_FOUND", "Job with ID '" + job_id + "' not found", 404);
            return;
        }
        
        std::string current_status = jobs_db[job_id]["status"];
        if (current_status == "completed" || current_status == "failed_permanently" || current_status == "cancelled") {
            EnhancedEndpoints::error_response(res, "INVALID_OPERATION", "Cannot cancel job in '" + current_status + "' state");
            return;
        }
        
        // Cancel the job
        jobs_db[job_id]["status"] = "cancelled";
        jobs_db[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Free up engine if assigned
        if (!jobs_db[job_id]["assigned_engine"].is_null()) {
            std::string engine_id = jobs_db[job_id]["assigned_engine"];
            if (engines_db.contains(engine_id)) {
                engines_db[engine_id]["status"] = "idle";
                engines_db[engine_id]["current_job_id"] = "";
            }
        }
        
        save_state_unlocked();
        
        nlohmann::json response;
        response["message"] = "Job cancelled successfully";
        response["job_id"] = job_id;
        EnhancedEndpoints::success_response(res, response);
    });
    
    // Job re-submission endpoint
    svr.Post(R"(/api/v1/jobs/([a-fA-F0-9\-]{36})/retry)", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        std::string job_id = req.matches[1];
        
        std::lock_guard<std::mutex> lock(state_mutex);
        if (!jobs_db.contains(job_id)) {
            EnhancedEndpoints::error_response(res, "NOT_FOUND", "Job with ID '" + job_id + "' not found", 404);
            return;
        }
        
        std::string current_status = jobs_db[job_id]["status"];
        if (current_status != "failed" && current_status != "failed_permanently") {
            EnhancedEndpoints::error_response(res, "INVALID_OPERATION", "Can only retry failed jobs, current status: " + current_status);
            return;
        }
        
        // Reset job for retry
        jobs_db[job_id]["status"] = "pending";
        jobs_db[job_id]["assigned_engine"] = nullptr;
        jobs_db[job_id]["output_url"] = nullptr;
        jobs_db[job_id]["retry_state"] = "none";
        jobs_db[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        save_state_unlocked();
        
        nlohmann::json response;
        response["message"] = "Job queued for retry";
        response["job_id"] = job_id;
        response["new_status"] = "pending";
        EnhancedEndpoints::success_response(res, response);
    });
    
    // Job completion endpoint
    svr.Post(R"(/api/v1/jobs/([a-fA-F0-9\\-]{36})/complete)", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        if (!EnhancedEndpoints::validate_json_content_type(req, res)) return;
        
        std::string job_id = req.matches[1];
        
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            
            if (!request_json.contains("output_url") || !request_json["output_url"].is_string()) {
                EnhancedEndpoints::error_response(res, "VALIDATION_ERROR", "Field 'output_url' is required and must be a string");
                return;
            }
            
            std::lock_guard<std::mutex> lock(state_mutex);
            if (!jobs_db.contains(job_id)) {
                EnhancedEndpoints::error_response(res, "NOT_FOUND", "Job with ID '" + job_id + "' not found", 404);
                return;
            }
            
            std::string current_status = jobs_db[job_id]["status"];
            if (current_status == "completed" || current_status == "failed_permanently") {
                EnhancedEndpoints::error_response(res, "INVALID_OPERATION", "Job is already in a final state: " + current_status);
                return;
            }
            
            // Complete the job
            jobs_db[job_id]["status"] = "completed";
            jobs_db[job_id]["output_url"] = request_json["output_url"];
            jobs_db[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Free up the engine
            if (!jobs_db[job_id]["assigned_engine"].is_null()) {
                std::string engine_id = jobs_db[job_id]["assigned_engine"];
                if (engines_db.contains(engine_id)) {
                    engines_db[engine_id]["status"] = "idle";
                    engines_db[engine_id]["current_job_id"] = "";
                }
            }
            
            save_state_unlocked();
            
            nlohmann::json response;
            response["message"] = "Job completed successfully";
            response["job_id"] = job_id;
            response["output_url"] = request_json["output_url"];
            EnhancedEndpoints::success_response(res, response);
            
        } catch (const nlohmann::json::parse_error& e) {
            EnhancedEndpoints::error_response(res, "JSON_PARSE_ERROR", "Invalid JSON: " + std::string(e.what()));
        } catch (const std::exception& e) {
            EnhancedEndpoints::error_response(res, "INTERNAL_ERROR", "Server error: " + std::string(e.what()), 500);
        }
    });
    
    // Job failure endpoint
    svr.Post(R"(/api/v1/jobs/([a-fA-F0-9\\-]{36})/fail)", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        if (!EnhancedEndpoints::validate_json_content_type(req, res)) return;
        
        std::string job_id = req.matches[1];
        
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            
            if (!request_json.contains("error_message")) {
                EnhancedEndpoints::error_response(res, "VALIDATION_ERROR", "Field 'error_message' is required");
                return;
            }
            
            std::lock_guard<std::mutex> lock(state_mutex);
            if (!jobs_db.contains(job_id)) {
                EnhancedEndpoints::error_response(res, "NOT_FOUND", "Job with ID '" + job_id + "' not found", 404);
                return;
            }
            
            std::string current_status = jobs_db[job_id]["status"];
            if (current_status == "completed" || current_status == "failed_permanently") {
                EnhancedEndpoints::error_response(res, "INVALID_OPERATION", "Job is already in a final state: " + current_status);
                return;
            }
            
            // Increment retries
            jobs_db[job_id]["retries"] = jobs_db[job_id].value("retries", 0) + 1;
            jobs_db[job_id]["error_message"] = request_json["error_message"];
            jobs_db[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Check if we should retry or fail permanently
            int current_retries = jobs_db[job_id]["retries"];
            int max_retries = jobs_db[job_id].value("max_retries", 3);
            
            if (current_retries < max_retries) {
                // Re-queue for retry
                jobs_db[job_id]["status"] = "pending";
                jobs_db[job_id]["assigned_engine"] = nullptr;
                jobs_db[job_id]["retry_state"] = "retry_scheduled";
            } else {
                // Fail permanently
                jobs_db[job_id]["status"] = "failed_permanently";
                jobs_db[job_id]["retry_state"] = "failed_retry";
            }
            
            // Free up the engine
            if (!jobs_db[job_id]["assigned_engine"].is_null()) {
                std::string engine_id = jobs_db[job_id]["assigned_engine"];
                if (engines_db.contains(engine_id)) {
                    engines_db[engine_id]["status"] = "idle";
                    engines_db[engine_id]["current_job_id"] = "";
                }
            }
            
            save_state_unlocked();
            
            nlohmann::json response;
            response["message"] = (current_retries < max_retries) ? "Job queued for retry" : "Job failed permanently";
            response["job_id"] = job_id;
            response["status"] = jobs_db[job_id]["status"];
            response["retries"] = current_retries;
            response["max_retries"] = max_retries;
            EnhancedEndpoints::success_response(res, response);
            
        } catch (const nlohmann::json::parse_error& e) {
            EnhancedEndpoints::error_response(res, "JSON_PARSE_ERROR", "Invalid JSON: " + std::string(e.what()));
        } catch (const std::exception& e) {
            EnhancedEndpoints::error_response(res, "INTERNAL_ERROR", "Server error: " + std::string(e.what()), 500);
        }
    });
    
    /*
    // Legacy endpoint: Job completion (for backward compatibility)
    svr.Post(R"(/jobs/([a-fA-F0-9\\-]{36})/complete)", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        std::string job_id = req.matches[1];
        
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            
            if (!request_json.contains("output_url") || !request_json["output_url"].is_string()) {
                res.status = 400;
                res.set_content("Bad Request: 'output_url' must be a string.", "text/plain");
                return;
            }
            
            std::lock_guard<std::mutex> lock(state_mutex);
            if (!jobs_db.contains(job_id)) {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
                return;
            }
            
            std::string current_status = jobs_db[job_id]["status"];
            if (current_status == "completed" || current_status == "failed_permanently") {
                res.status = 400;
                res.set_content("Bad Request: Job is already in a final state.", "text/plain");
                return;
            }
            
            // Complete the job
            jobs_db[job_id]["status"] = "completed";
            jobs_db[job_id]["output_url"] = request_json["output_url"];
            jobs_db[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Free up the engine
            if (jobs_db[job_id].contains("assigned_engine") && !jobs_db[job_id]["assigned_engine"].is_null()) {
                std::string engine_id = jobs_db[job_id]["assigned_engine"];
                if (engines_db.contains(engine_id)) {
                    engines_db[engine_id]["status"] = "idle";
                }
            }
            
            save_state_unlocked();
            res.set_content("Job " + job_id + " marked as completed", "text/plain");
            
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });
    
    // Legacy endpoint: Job failure (for backward compatibility)
    svr.Post(R"(/jobs/([a-fA-F0-9\\-]{36})/fail)", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        std::string job_id = req.matches[1];
        
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            
            if (!request_json.contains("error_message")) {
                res.status = 400;
                res.set_content("Bad Request: 'error_message' is missing.", "text/plain");
                return;
            }
            
            std::lock_guard<std::mutex> lock(state_mutex);
            if (!jobs_db.contains(job_id)) {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
                return;
            }
            
            std::string current_status = jobs_db[job_id]["status"];
            if (current_status == "completed" || current_status == "failed_permanently") {
                res.status = 400;
                res.set_content("Bad Request: Job is already in a final state.", "text/plain");
                return;
            }
            
            // Increment retries
            jobs_db[job_id]["retries"] = jobs_db[job_id].value("retries", 0) + 1;
            jobs_db[job_id]["error_message"] = request_json["error_message"];
            jobs_db[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Check if we should retry or fail permanently
            int current_retries = jobs_db[job_id]["retries"];
            int max_retries = jobs_db[job_id].value("max_retries", 3);
            
            if (current_retries < max_retries) {
                // Re-queue for retry
                jobs_db[job_id]["status"] = "pending";
                jobs_db[job_id]["assigned_engine"] = nullptr;
                res.set_content("Job " + job_id + " re-queued", "text/plain");
            } else {
                // Fail permanently
                jobs_db[job_id]["status"] = "failed_permanently";
                res.set_content("Job " + job_id + " failed permanently", "text/plain");
            }
            
            save_state_unlocked();
            
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });
    */
}

// Enhanced system endpoints
void setup_enhanced_system_endpoints(httplib::Server &svr, const std::string& api_key) {
    
    // Version endpoint
    svr.Get("/api/v1/version", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json version_info;
        version_info["version"] = "2.0.0";
        version_info["api_version"] = "v1";
        version_info["build_time"] = __DATE__ " " __TIME__;
        EnhancedEndpoints::success_response(res, version_info);
    });
    
    // Enhanced status endpoint
    svr.Get("/api/v1/status", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(state_mutex);
        
        nlohmann::json status;
        status["status"] = "healthy";
        status["version"] = "2.0.0";
        status["api_version"] = "v1";
        status["jobs_total"] = jobs_db.size();
        status["engines_total"] = engines_db.size();
        
        // Detailed job statistics
        nlohmann::json job_stats;
        for (const auto& [job_id, job_data] : jobs_db.items()) {
            std::string job_status = job_data.value("status", "unknown");
            job_stats[job_status] = job_stats.value(job_status, 0) + 1;
        }
        status["job_statistics"] = job_stats;
        
        // Engine statistics
        nlohmann::json engine_stats;
        int idle_engines = 0, busy_engines = 0;
        for (const auto& [engine_id, engine_data] : engines_db.items()) {
            std::string engine_status = engine_data.value("status", "unknown");
            if (engine_status == "idle") idle_engines++;
            else if (engine_status == "busy") busy_engines++;
        }
        engine_stats["idle"] = idle_engines;
        engine_stats["busy"] = busy_engines;
        status["engine_statistics"] = engine_stats;
        
        EnhancedEndpoints::success_response(res, status);
    });
    
    // Engine deregistration endpoint
    svr.Delete(R"(/api/v1/engines/([a-zA-Z0-9\-_]+))", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        std::string engine_id = req.matches[1];
        
        std::lock_guard<std::mutex> lock(state_mutex);
        if (!engines_db.contains(engine_id)) {
            EnhancedEndpoints::error_response(res, "NOT_FOUND", "Engine with ID '" + engine_id + "' not found", 404);
            return;
        }
        
        // Free up any assigned jobs
        for (auto& [job_id, job_data] : jobs_db.items()) {
            if (!job_data["assigned_engine"].is_null() && job_data["assigned_engine"] == engine_id) {
                job_data["status"] = "pending";
                job_data["assigned_engine"] = nullptr;
                job_data["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
            }
        }
        
        // Remove engine
        engines_db.erase(engine_id);
        save_state_unlocked();
        
        nlohmann::json response;
        response["message"] = "Engine deregistered successfully";
        response["engine_id"] = engine_id;
        EnhancedEndpoints::success_response(res, response);
    });
    
    // Legacy endpoints for backward compatibility
    
    // COMMENTED OUT: Legacy job submission endpoint - now handled by JobSubmissionHandler
    // The new handler-based implementation in dispatch_server_core.cpp provides better
    // validation including negative number checks that this legacy version lacked
    /*
    svr.Post("/jobs/", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            
            if (!request_json.contains("source_url") || !request_json["source_url"].is_string()) {
                res.status = 400;
                res.set_content("Bad Request: 'source_url' is missing or not a string.", "text/plain");
                return;
            }
            if (!request_json.contains("target_codec") || !request_json["target_codec"].is_string()) {
                res.status = 400;
                res.set_content("Bad Request: 'target_codec' is missing or not a string.", "text/plain");
                return;
            }
            if (request_json.contains("job_size") && !request_json["job_size"].is_number()) {
                res.status = 400;
                res.set_content("Bad Request: 'job_size' must be a number.", "text/plain");
                return;
            }
            if (request_json.contains("max_retries") && !request_json["max_retries"].is_number_integer()) {
                res.status = 400;
                res.set_content("Bad Request: 'max_retries' must be an integer.", "text/plain");
                return;
            }
            
            std::string job_id = generate_uuid();
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            
            nlohmann::json job;
            job["job_id"] = job_id;
            job["source_url"] = request_json["source_url"];
            job["target_codec"] = request_json["target_codec"];
            job["job_size"] = request_json.value("job_size", 0.0);
            job["status"] = "pending";
            job["assigned_engine"] = nullptr;
            job["output_url"] = nullptr;
            job["retries"] = 0;
            job["max_retries"] = request_json.value("max_retries", 3);
            job["priority"] = request_json.value("priority", 0);
            job["created_at"] = now_ms;
            job["updated_at"] = now_ms;
            
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                jobs_db[job_id] = job;
                save_state_unlocked();
            }
            
            res.set_content(job.dump(), "application/json");
            
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });
    */
    
    // Legacy: Job status
    svr.Get(R"(/jobs/([a-fA-F0-9\\-]{36}))", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        std::string job_id = req.matches[1];
        std::lock_guard<std::mutex> lock(state_mutex);
        if (jobs_db.contains(job_id)) {
            res.set_content(jobs_db[job_id].dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content("Job not found", "text/plain");
        }
    });
    
    /*
    // Legacy: All jobs
    svr.Get("/jobs/", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        nlohmann::json all_jobs = nlohmann::json::array();
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            for (const auto& [key, val] : jobs_db.items()) {
                all_jobs.push_back(val);
            }
        }
        res.set_content(all_jobs.dump(), "application/json");
    });
    */
    
    /*
    // Legacy: All engines
    svr.Get("/engines/", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        nlohmann::json all_engines = nlohmann::json::array();
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            for (const auto& [key, val] : engines_db.items()) {
                all_engines.push_back(val);
            }
        }
        res.set_content(all_engines.dump(), "application/json");
    });
    
    // Legacy: Engine heartbeat
    svr.Post("/engines/heartbeat", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (!request_json.contains("engine_id") || !request_json["engine_id"].is_string()) {
                res.status = 400;
                res.set_content("Bad Request: 'engine_id' is missing or not a string.", "text/plain");
                return;
            }
            
            // Validate storage_capacity_gb if present
            if (request_json.contains("storage_capacity_gb")) {
                if (!request_json["storage_capacity_gb"].is_number()) {
                    res.status = 400;
                    res.set_content("Bad Request: 'storage_capacity_gb' must be a number.", "text/plain");
                    return;
                }
                if (request_json["storage_capacity_gb"].get<double>() < 0) {
                    res.status = 400;
                    res.set_content("Bad Request: 'storage_capacity_gb' must be non-negative.", "text/plain");
                    return;
                }
            }

            // Validate can_stream if present
            if (request_json.contains("can_stream") && !request_json["can_stream"].is_boolean()) {
                res.status = 400;
                res.set_content("Bad Request: 'can_stream' must be a boolean.", "text/plain");
                return;
            }
            
            std::string engine_id = request_json["engine_id"];
            
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                if (engines_db.contains(engine_id)) {
                     // Engine found
                } else {
                     // Engine not found
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                engines_db[engine_id] = request_json;
                save_state_unlocked();
            }
            res.set_content("Heartbeat received from engine " + engine_id, "text/plain");
            
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });
    
    // Legacy: Engine benchmark
    svr.Post("/engines/benchmark_result", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            std::string engine_id = request_json["engine_id"];
            
            // Validate benchmark_time if present
            if (request_json.contains("benchmark_time")) {
                 if (!request_json["benchmark_time"].is_number()) {
                    res.status = 400;
                    res.set_content("Bad Request: 'benchmark_time' must be a number.", "text/plain");
                    return;
                }
                if (request_json["benchmark_time"].get<double>() < 0) {
                    res.status = 400;
                    res.set_content("Bad Request: 'benchmark_time' must be non-negative.", "text/plain");
                    return;
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                if (engines_db.contains(engine_id)) {
                     // Engine found
                } else {
                     // Engine not found
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                if (engines_db.contains(engine_id)) {
                    engines_db[engine_id]["benchmark_time"] = request_json["benchmark_time"];
                } else {
                    res.status = 404;
                    res.set_content("Engine not found", "text/plain");
                    return;
                }
            }
            res.set_content("Benchmark result received from engine " + engine_id, "text/plain");
            
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });
    
    // Legacy: Job assignment
    svr.Post("/assign_job/", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (!EnhancedEndpoints::validate_api_key(req, res, api_key)) return;
        
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (!request_json.contains("engine_id") || !request_json["engine_id"].is_string()) {
                res.status = 400;
                res.set_content("Bad Request: 'engine_id' is missing or not a string.", "text/plain");
                return;
            }
            
            std::string engine_id = request_json["engine_id"];
            std::lock_guard<std::mutex> lock(state_mutex);
            
            if (!engines_db.contains(engine_id)) {
                res.status = 404;
                res.set_content("Engine not found", "text/plain");
                return;
            }
            
            // Check if engine is busy
            if (engines_db[engine_id].value("status", "idle") == "busy") {
                res.status = 204;
                res.set_content("Engine is busy", "text/plain");
                return;
            }
            
            // Find a pending job
            for (auto& [job_id, job] : jobs_db.items()) {
                if (job["status"] == "pending") {
                    job["status"] = "assigned";
                    job["assigned_engine"] = engine_id;
                    job["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    
                    engines_db[engine_id]["status"] = "busy";
                    save_state_unlocked();
                    
                    res.set_content(job.dump(), "application/json");
                    return;
                }
            }
            
            // No jobs pending
            res.status = 204;
            
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });
    */
}