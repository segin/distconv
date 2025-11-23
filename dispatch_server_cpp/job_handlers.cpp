#include "job_handlers.h"
#include "dispatch_server_core.h"
#include "dispatch_server_constants.h"
#include <chrono>
#include <mutex>

// External globals (from dispatch_server_core.cpp)
extern nlohmann::json jobs_db;
extern std::mutex state_mutex;
extern void save_state_unlocked();
extern std::string generate_uuid();

using namespace Constants;

JobSubmissionHandler::JobSubmissionHandler(std::shared_ptr<AuthMiddleware> auth)
    : auth_(auth) {}

void JobSubmissionHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // 1. Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }
    
    // 2. Parse JSON
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_error_response(res, "Invalid JSON: " + std::string(e.what()), 400);
        return;
    }
    
    // 3. Validate input
    if (!validate_job_input(request_json, res)) {
        return;
    }
    
    // 4. Create and save job
    try {
        nlohmann::json job = create_job(request_json);
        
        // Save to database with lock
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            jobs_db[job["job_id"]] = job;
            save_state_unlocked();
        }
        
        // Return success response
        set_json_response(res, job, 200);
        
    } catch (const nlohmann::json::type_error& e) {
        set_error_response(res, "Bad Request: " + std::string(e.what()), 400);
    } catch (const std::exception& e) {
        set_error_response(res, "Server error: " + std::string(e.what()), 500);
    }
}

bool JobSubmissionHandler::validate_job_input(const nlohmann::json& input, 
                                                httplib::Response& res) {
    // Validate source_url
    if (!input.contains("source_url") || !input["source_url"].is_string()) {
        set_error_response(res, 
            "Bad Request: 'source_url' is missing or not a string.", 400);
        return false;
    }
    
    // Validate target_codec
    if (!input.contains("target_codec") || !input["target_codec"].is_string()) {
        set_error_response(res, 
            "Bad Request: 'target_codec' is missing or not a string.", 400);
        return false;
    }
    
    // Validate job_size if present
    if (input.contains("job_size")) {
        if (!input["job_size"].is_number()) {
            set_error_response(res, 
                "Bad Request: 'job_size' must be a number.", 400);
            return false;
        }
        if (input["job_size"].get<double>() < 0) {
            set_error_response(res, 
                "Bad Request: 'job_size' must be a non-negative number.", 400);
            return false;
        }
    }
    
    // Validate max_retries if present
    if (input.contains("max_retries")) {
        if (!input["max_retries"].is_number_integer()) {
            set_error_response(res, 
                "Bad Request: 'max_retries' must be an integer.", 400);
            return false;
        }
        if (input["max_retries"].get<int>() < 0) {
            set_error_response(res, 
                "Bad Request: 'max_retries' must be a non-negative integer.", 400);
            return false;
        }
    }
    
    return true;
}

nlohmann::json JobSubmissionHandler::create_job(const nlohmann::json& input) {
    std::string job_id = generate_uuid();
    
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    nlohmann::json job;
    job["job_id"] = job_id;
    job["source_url"] = input["source_url"];
    job["target_codec"] = input["target_codec"];
    job["job_size"] = input.value("job_size", 0.0);
    job["status"] = "pending";
    job["assigned_engine"] = nullptr;
    job["output_url"] = nullptr;
    job["retries"] = 0;
    job["max_retries"] = input.value("max_retries", DEFAULT_MAX_RETRIES);
    job["priority"] = input.value("priority", PRIORITY_NORMAL);
    job["created_at"] = now_ms;
    job["updated_at"] = now_ms;
    
    return job;
}

JobStatusHandler::JobStatusHandler(std::shared_ptr<AuthMiddleware> auth)
    : auth_(auth) {}

void JobStatusHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // 1. Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // 2. Get Job ID
    // Note: req.matches is populated by httplib when using regex paths
    if (req.matches.size() < 2) {
        set_error_response(res, "Internal Server Error: Job ID not found in path", 500);
        return;
    }
    std::string job_id = req.matches[1];

    // 3. Retrieve Job
    std::lock_guard<std::mutex> lock(state_mutex);
    if (jobs_db.contains(job_id)) {
        set_json_response(res, jobs_db[job_id], 200);
    } else {
        res.status = 404;
        res.set_content("Job not found", "text/plain");
    }
}
