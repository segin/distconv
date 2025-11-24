#include "job_action_handlers.h"
#include "dispatch_server_core.h"
#include <mutex>
#include <regex>

// ==================== JobCompletionHandler ====================

JobCompletionHandler::JobCompletionHandler(std::shared_ptr<AuthMiddleware> auth)
    : auth_(auth) {}

void JobCompletionHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // 1. Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // 2. Get Job ID from path
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Internal Server Error: Job ID not found in path", "server_error", 500);
        return;
    }
    std::string job_id = req.matches[1];

    // 3. Parse JSON
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400, e.what());
        return;
    }

    // 4. Validate output_url
    if (!request_json.contains("output_url") || !request_json["output_url"].is_string()) {
        set_json_error_response(res, "Bad Request: 'output_url' must be a string.", "validation_error", 400);
        return;
    }
    
    std::string output_url = request_json["output_url"];
    std::regex url_regex(R"(^https?://.+)");
    if (!std::regex_match(output_url, url_regex)) {
        set_json_error_response(res, "Bad Request: 'output_url' must be a valid URL starting with http:// or https://", "validation_error", 400);
        return;
    }

    // 5. Update Job Status
    try {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (jobs_db.contains(job_id)) {
            jobs_db[job_id]["status"] = "completed";
            jobs_db[job_id]["output_url"] = request_json["output_url"];
            
            // Free up the engine that was working on this job
            if (jobs_db[job_id].contains("assigned_engine") && !jobs_db[job_id]["assigned_engine"].is_null()) {
                std::string engine_id = jobs_db[job_id]["assigned_engine"];
                if (engines_db.contains(engine_id)) {
                    engines_db[engine_id]["status"] = "idle";
                }
            }
            save_state_unlocked();
        } else {
            res.status = 404;
            res.set_content("Job not found", "text/plain");
            return;
        }
    } catch (const std::exception& e) {
        set_json_error_response(res, "Internal server error", "server_error", 500, e.what());
        return;
    }

    // 6. Return Success
    res.set_content("Job " + job_id + " marked as completed", "text/plain");
}

// ==================== JobFailureHandler ====================

JobFailureHandler::JobFailureHandler(std::shared_ptr<AuthMiddleware> auth)
    : auth_(auth) {}

void JobFailureHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // 1. Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // 2. Get Job ID from path
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Internal Server Error: Job ID not found in path", "server_error", 500);
        return;
    }
    std::string job_id = req.matches[1];

    // 3. Parse JSON
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400, e.what());
        return;
    }

    // 4. Validate error_message
    if (!request_json.contains("error_message")) {
        set_json_error_response(res, "Bad Request: 'error_message' is missing.", "validation_error", 400);
        return;
    }

    // 5. Update Job Status with Retry Logic
    try {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (jobs_db.contains(job_id)) {
            // Check if job is already in a final state
            if (jobs_db[job_id]["status"] == "completed" || jobs_db[job_id]["status"] == "failed_permanently") {
                set_json_error_response(res, "Bad Request: Job is already in a final state.", "validation_error", 400, "Job ID: " + job_id);
                return;
            }

            // Increment retries
            jobs_db[job_id]["retries"] = jobs_db[job_id].value("retries", 0) + 1;
            
            // Check if we should retry or fail permanently
            int current_retries = jobs_db[job_id]["retries"];
            int max_retries = jobs_db[job_id].value("max_retries", 3);
            
            if (current_retries < max_retries) {
                // Re-queue for retry
                jobs_db[job_id]["status"] = "pending";
                jobs_db[job_id]["error_message"] = request_json["error_message"];
                save_state_unlocked();
                res.set_content("Job " + job_id + " re-queued", "text/plain");
            } else {
                // Fail permanently
                jobs_db[job_id]["status"] = "failed_permanently";
                jobs_db[job_id]["error_message"] = request_json["error_message"];
                save_state_unlocked();
                res.set_content("Job " + job_id + " failed permanently", "text/plain");
            }
        } else {
            res.status = 404;
            res.set_content("Job not found", "text/plain");
            return;
        }
    } catch (const std::exception& e) {
        set_json_error_response(res, "Internal server error", "server_error", 500, e.what());
        return;
    }
}
