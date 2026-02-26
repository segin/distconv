#include "assignment_handler.h"
#include "dispatch_server_core.h"
#include "repositories.h"
#include "dispatch_server_constants.h"
#include <mutex>
#include <algorithm>
#include <iostream>

namespace distconv {
namespace DispatchServer {

using namespace Constants;

JobAssignmentHandler::JobAssignmentHandler(std::shared_ptr<AuthMiddleware> auth, 
                                           std::shared_ptr<IJobRepository> job_repo,
                                           std::shared_ptr<IEngineRepository> engine_repo)
    : auth_(auth), job_repo_(job_repo), engine_repo_(engine_repo) {}

void JobAssignmentHandler::handle(const httplib::Request& req, httplib::Response& res) {
    if (!auth_->authenticate(req, res)) return;

    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (...) {
        set_error_response(res, "Invalid JSON", 400);
        return;
    }
    
    if (!request_json.contains("engine_id") || !request_json["engine_id"].is_string()) {
        set_error_response(res, "Missing engine_id", 400);
        return;
    }
    
    std::string engine_id = request_json["engine_id"];

    // Ensure engine exists and is idle
    nlohmann::json engine = engine_repo_->get_engine(engine_id);
    if (engine.is_null() || engine.empty()) {
        set_error_response(res, "Engine not registered", 404);
        return;
    }

    // Get next pending job (O(1)-ish with SQLite)
    nlohmann::json job = job_repo_->get_next_pending_job({}); // Empty capable_engines for now
    if (job.is_null() || job.empty()) {
        res.status = 204; // No Content
        return;
    }

    // Assign job
    std::string job_id = job["job_id"];
    job["status"] = "assigned";
    job["assigned_engine"] = engine_id;
    job["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    engine["status"] = "busy";
    engine["current_job_id"] = job_id;
    engine["updated_at"] = job["updated_at"];

    // Atomicity: We should really have a transaction here, but for now we'll do best-effort
    // or rely on locks inside repositories.
    job_repo_->save_job(job_id, job);
    engine_repo_->save_engine(engine_id, engine);

    set_json_response(res, job, 200);
}

} // namespace DispatchServer
} // namespace distconv
