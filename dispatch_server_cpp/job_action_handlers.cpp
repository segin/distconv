#include "job_action_handlers.h"
#include "dispatch_server_core.h"
#include <mutex>
#include <chrono>

namespace distconv {
namespace DispatchServer {

JobCompletionHandler::JobCompletionHandler(std::shared_ptr<AuthMiddleware> auth, 
                                           std::shared_ptr<IJobRepository> job_repo,
                                           std::shared_ptr<IEngineRepository> engine_repo)
    : auth_(auth), job_repo_(job_repo), engine_repo_(engine_repo) {}

void JobCompletionHandler::handle(const httplib::Request& req, httplib::Response& res) {
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

    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (...) {}

    job["status"] = "completed";
    job["output_url"] = request_json.value("output_url", "");
    job["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (!job["assigned_engine"].is_null()) {
        std::string engine_id = job["assigned_engine"];
        nlohmann::json engine = engine_repo_->get_engine(engine_id);
        if (!engine.is_null()) {
            engine["status"] = "idle";
            engine["current_job_id"] = "";
            engine_repo_->save_engine(engine_id, engine);
        }
        job["assigned_engine"] = nullptr;
    }

    job_repo_->save_job(job_id, job);
    set_json_response(res, job, 200);
}

JobFailureHandler::JobFailureHandler(std::shared_ptr<AuthMiddleware> auth, 
                                     std::shared_ptr<IJobRepository> job_repo,
                                     std::shared_ptr<IEngineRepository> engine_repo)
    : auth_(auth), job_repo_(job_repo), engine_repo_(engine_repo) {}

void JobFailureHandler::handle(const httplib::Request& req, httplib::Response& res) {
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

    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (...) {}

    job["status"] = "failed_permanently";
    job["error_message"] = request_json.value("error_message", "Job reported failure");
    job["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (!job["assigned_engine"].is_null()) {
        std::string engine_id = job["assigned_engine"];
        nlohmann::json engine = engine_repo_->get_engine(engine_id);
        if (!engine.is_null()) {
            engine["status"] = "idle";
            engine["current_job_id"] = "";
            engine_repo_->save_engine(engine_id, engine);
        }
    }

    job_repo_->save_job(job_id, job);
    set_json_response(res, job, 200);
}

} // namespace DispatchServer
} // namespace distconv
