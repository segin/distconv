#include "job_handlers.h"
#include "dispatch_server_core.h"
#include "dispatch_server_constants.h"
#include <chrono>
#include <mutex>

namespace distconv {
namespace DispatchServer {

// External globals (to be removed)
extern std::string generate_uuid();

using namespace Constants;

JobSubmissionHandler::JobSubmissionHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo)
    : auth_(auth), job_repo_(job_repo) {}

void JobSubmissionHandler::handle(const httplib::Request& req, httplib::Response& res) {
    if (!auth_->authenticate(req, res)) return;
    
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400, e.what());
        return;
    }
    
    if (!validate_job_input(request_json, res)) return;
    
    try {
        nlohmann::json job = create_job(request_json);
        job_repo_->save_job(job["job_id"], job);
        set_json_response(res, job, 200);
    } catch (const std::exception& e) {
        set_json_error_response(res, "Internal server error", "server_error", 500, e.what());
    }
}

bool JobSubmissionHandler::validate_job_input(const nlohmann::json& input, httplib::Response& res) {
    if (!input.contains("source_url") || !input["source_url"].is_string()) {
        set_json_error_response(res, "Bad Request: 'source_url' is missing or not a string.", "validation_error", 400);
        return false;
    }
    if (!input.contains("target_codec") || !input["target_codec"].is_string()) {
        set_json_error_response(res, "Bad Request: 'target_codec' is missing or not a string.", "validation_error", 400);
        return false;
    }
    return true;
}

nlohmann::json JobSubmissionHandler::create_job(const nlohmann::json& input) {
    std::string job_id = generate_uuid();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
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
    job["resource_requirements"] = input.value("resource_requirements", nlohmann::json::object());
    job["created_at"] = now_ms;
    job["updated_at"] = now_ms;
    
    return job;
}

JobStatusHandler::JobStatusHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo)
    : auth_(auth), job_repo_(job_repo) {}

void JobStatusHandler::handle(const httplib::Request& req, httplib::Response& res) {
    if (!auth_->authenticate(req, res)) return;
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Internal Server Error: Job ID not found in path", "server_error", 500);
        return;
    }
    std::string job_id = req.matches[1];
    nlohmann::json job = job_repo_->get_job(job_id);
    if (!job.is_null() && !job.empty()) {
        set_json_response(res, job, 200);
    } else {
        res.status = 404;
        res.set_content("Job not found", "text/plain");
    }
}

JobListHandler::JobListHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo)
    : auth_(auth), job_repo_(job_repo) {}

void JobListHandler::handle(const httplib::Request& req, httplib::Response& res) {
    if (!auth_->authenticate(req, res)) return;
    auto all_jobs_vec = job_repo_->get_all_jobs();
    nlohmann::json all_jobs = nlohmann::json::array();
    for (const auto& job : all_jobs_vec) {
        all_jobs.push_back(job);
    }
    set_json_response(res, all_jobs, 200);
}

JobRetryHandler::JobRetryHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo)
    : auth_(auth), job_repo_(job_repo) {}

void JobRetryHandler::handle(const httplib::Request& req, httplib::Response& res) {
    if (!auth_->authenticate(req, res)) return;
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Internal Server Error: Job ID not found in path", "server_error", 500);
        return;
    }
    std::string job_id = req.matches[1];
    nlohmann::json job = job_repo_->get_job(job_id);
    if (job.is_null() || job.empty()) {
        res.status = 404;
        res.set_content("Job not found", "text/plain");
        return;
    }

    std::string status = job.value("status", "");
    if (status == "failed" || status == "failed_permanently" || status == "failed_retry" || status == "cancelled") {
        job["status"] = "pending";
        job["retries"] = 0;
        job["assigned_engine"] = nullptr;
        job["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        job_repo_->save_job(job_id, job);
        set_json_response(res, job, 200);
    } else {
        set_json_error_response(res, "Job is not in a failed or cancelled state", "validation_error", 400, "Job ID: " + job_id);
    }
}

JobCancelHandler::JobCancelHandler(std::shared_ptr<AuthMiddleware> auth, 
                                   std::shared_ptr<IJobRepository> job_repo,
                                   std::shared_ptr<IEngineRepository> engine_repo)
    : auth_(auth), job_repo_(job_repo), engine_repo_(engine_repo) {}

void JobCancelHandler::handle(const httplib::Request& req, httplib::Response& res) {
    if (!auth_->authenticate(req, res)) return;
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Internal Server Error: Job ID not found in path", "server_error", 500);
        return;
    }
    std::string job_id = req.matches[1];
    nlohmann::json job = job_repo_->get_job(job_id);
    if (job.is_null() || job.empty()) {
        res.status = 404;
        res.set_content("Job not found", "text/plain");
        return;
    }

    std::string status = job.value("status", "");
    if (status != "completed" && status != "failed_permanently" && status != "cancelled") {
        job["status"] = "cancelled";
        job["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        
        if (!job["assigned_engine"].is_null()) {
            std::string engine_id = job["assigned_engine"];
            nlohmann::json engine = engine_repo_->get_engine(engine_id);
            if (!engine.is_null() && !engine.empty()) {
                engine["status"] = "idle";
                engine["current_job_id"] = "";
                engine_repo_->save_engine(engine_id, engine);
            }
            job["assigned_engine"] = nullptr;
        }
        job_repo_->save_job(job_id, job);
        set_json_response(res, job, 200);
    } else {
        set_json_error_response(res, "Job cannot be cancelled in current state", "validation_error", 400, "Current status: " + status);
    }
}

} // namespace DispatchServer
} // namespace distconv
