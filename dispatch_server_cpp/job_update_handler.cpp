#include "job_update_handler.h"
#include "dispatch_server_core.h"
#include "dispatch_server_constants.h"
#include <regex>

namespace distconv {
namespace DispatchServer {

using namespace Constants;

// JobUpdateHandler implementation
JobUpdateHandler::JobUpdateHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo)
    : auth_(auth), job_repo_(job_repo) {}

void JobUpdateHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // Extract job ID from URL
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Invalid or missing job ID in URL", "validation_error", 400);
        return;
    }
    std::string job_id = req.matches[1];

    // Check if job exists
    if (!job_repo_->job_exists(job_id)) {
        set_json_error_response(res, "Job not found", "not_found", 404, "Job ID: " + job_id);
        return;
    }

    // Parse request body
    nlohmann::json updates;
    try {
        updates = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400, e.what());
        return;
    }

    // Update the job
    if (job_repo_->update_job(job_id, updates)) {
        nlohmann::json updated_job = job_repo_->get_job(job_id);
        set_json_response(res, updated_job, 200);
    } else {
        set_json_error_response(res, "Failed to update job", "update_error", 500, "Job ID: " + job_id);
    }
}

// EngineJobsHandler implementation
EngineJobsHandler::EngineJobsHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo)
    : auth_(auth), job_repo_(job_repo) {}

void EngineJobsHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // Extract engine ID from URL
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Invalid or missing engine ID in URL", "validation_error", 400);
        return;
    }
    std::string engine_id = req.matches[1];

    // Get jobs for this engine
    std::vector<nlohmann::json> jobs = job_repo_->get_jobs_by_engine(engine_id);
    
    nlohmann::json response = nlohmann::json::array();
    for (const auto& job : jobs) {
        response.push_back(job);
    }

    set_json_response(res, response, 200);
}

// JobUnified StatusHandler implementation
JobUnifiedStatusHandler::JobUnifiedStatusHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo)
    : auth_(auth), job_repo_(job_repo) {}

void JobUnifiedStatusHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // Extract job ID from URL
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Invalid or missing job ID in URL", "validation_error", 400);
        return;
    }
    std::string job_id = req.matches[1];

    // Check if job exists
    if (!job_repo_->job_exists(job_id)) {
        set_json_error_response(res, "Job not found", "not_found", 404, "Job ID: " + job_id);
        return;
    }

    // Parse request body
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400, e.what());
        return;
    }

    // Validate status field
    if (!request_json.contains("status") || !request_json["status"].is_string()) {
        set_json_error_response(res, "Missing or invalid 'status' field", "validation_error", 400);
        return;
    }

    std::string new_status = request_json["status"];
    if (new_status != "completed" && new_status != "failed") {
        set_json_error_response(res, "Status must be 'completed' or 'failed'", "validation_error", 400);
        return;
    }

    // Get the job and update it
    nlohmann::json job = job_repo_->get_job(job_id);
    job["status"] = new_status;
    job["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (new_status == "completed" && request_json.contains("output_url")) {
        job["output_url"] = request_json["output_url"];
    }

    if (new_status == "failed") {
        if (request_json.contains("error_message")) {
            job["error_message"] = request_json["error_message"];
        }
        
        // Handle retry logic
        int retries = job.value("retries", 0);
        int max_retries = job.value("max_retries", MAX_RETRIES);
        
        if (retries < max_retries) {
            // Mark for retry with exponential backoff
            int backoff_seconds = RETRY_DELAY_BASE_SECONDS * (1 << retries);
            auto retry_time = std::chrono::system_clock::now() + std::chrono::seconds(backoff_seconds);
            int64_t retry_after = std::chrono::duration_cast<std::chrono::milliseconds>(
                retry_time.time_since_epoch()).count();
            
            job_repo_->mark_job_as_failed_retry(job_id, retry_after);
        } else {
            job["status"] = "failed_permanently";
        }
    }

    job_repo_->save_job(job_id, job);
    set_json_response(res, job, 200);
}

// JobProgressHandler implementation
JobProgressHandler::JobProgressHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo)
    : auth_(auth), job_repo_(job_repo) {}

void JobProgressHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // Extract job ID from URL
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Invalid or missing job ID in URL", "validation_error", 400);
        return;
    }
    std::string job_id = req.matches[1];

    // Check if job exists
    if (!job_repo_->job_exists(job_id)) {
        set_json_error_response(res, "Job not found", "not_found", 404, "Job ID: " + job_id);
        return;
    }

    // Parse request body
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400, e.what());
        return;
    }

    // Validate progress field
    if (!request_json.contains("progress") || !request_json["progress"].is_number()) {
        set_json_error_response(res, "Missing or invalid 'progress' field", "validation_error", 400);
        return;
    }

    int progress = request_json["progress"];
    if (progress < 0 || progress > 100) {
        set_json_error_response(res, "Progress must be between 0 and 100", "validation_error", 400);
        return;
    }

    std::string message = request_json.value("message", "");

    // Update progress
    if (job_repo_->update_job_progress(job_id, progress, message)) {
        nlohmann::json response;
        response["job_id"] = job_id;
        response["progress"] = progress;
        response["message"] = message;
        set_json_response(res, response, 200);
    } else {
        set_json_error_response(res, "Failed to update progress", "update_error", 500, "Job ID: " + job_id);
    }
}

} // namespace DispatchServer
} // namespace distconv
