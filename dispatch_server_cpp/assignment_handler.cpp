#include "assignment_handler.h"
#include "dispatch_server_core.h"
#include <mutex>
#include <algorithm>

// ==================== JobAssignmentHandler ====================

JobAssignmentHandler::JobAssignmentHandler(std::shared_ptr<AuthMiddleware> auth)
    : auth_(auth) {}

nlohmann::json JobAssignmentHandler::find_pending_job() {
    // Find first pending job without an assigned engine
    for (auto const& [job_key, job_val] : jobs_db.items()) {
        if (job_val["status"] == "pending" && job_val["assigned_engine"].is_null()) {
            return job_val;
        }
    }
    return nullptr;
}

std::vector<nlohmann::json> JobAssignmentHandler::find_available_engines() {
    std::vector<nlohmann::json> available_engines;
    for (auto const& [eng_id, eng_data] : engines_db.items()) {
        if (eng_data["status"] == "idle" && eng_data.contains("benchmark_time")) {
            available_engines.push_back(eng_data);
        }
    }
    
    // Sort engines by benchmark_time (faster engines first)
    std::sort(available_engines.begin(), available_engines.end(), 
        [](const nlohmann::json& a, const nlohmann::json& b) {
            return a["benchmark_time"] < b["benchmark_time"];
        });
    
    return available_engines;
}

nlohmann::json JobAssignmentHandler::select_engine_for_job(
    const nlohmann::json& job, 
    const std::vector<nlohmann::json>& engines) {
    
    if (engines.empty()) {
        return nullptr;
    }
    
    double job_size = job.value("job_size", 0.0);
    double large_job_threshold = 100.0;
    double small_job_threshold = 50.0;
    
    // For large jobs, prefer streaming-capable engines
    if (job_size >= large_job_threshold) {
        for (const auto& eng : engines) {
            if (eng.value("streaming_support", false)) {
                return eng;  // First streaming-capable engine (also fastest)
            }
        }
        // No streaming engine available, use fastest
        return engines[0];
    }
    
    // For small jobs, use slowest engine to save fast engines for big jobs
    if (job_size < small_job_threshold) {
        return engines.back();
    }
    
    // For medium jobs, use fastest engine
    return engines[0];
}

void JobAssignmentHandler::assign_job_to_engine(
    const nlohmann::json& job, 
    const nlohmann::json& engine) {
    
    std::string job_id = job["job_id"].get<std::string>();
    std::string engine_id = engine["engine_id"].get<std::string>();
    
    jobs_db[job_id]["status"] = "assigned";
    jobs_db[job_id]["assigned_engine"] = engine_id;
    engines_db[engine_id]["status"] = "busy";
}

void JobAssignmentHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // 1. Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // 2. Find Pending Job and Available Engines (with lock)
    std::lock_guard<std::mutex> lock(state_mutex);
    
    nlohmann::json pending_job = find_pending_job();
    if (pending_job.is_null()) {
        res.status = 204;  // No Content
        return;
    }
    
    std::vector<nlohmann::json> available_engines = find_available_engines();
    if (available_engines.empty()) {
        res.status = 204;  // No Content
        return;
    }
    
    // 3. Select Best Engine for Job
    nlohmann::json selected_engine = select_engine_for_job(pending_job, available_engines);
    if (selected_engine.is_null()) {
        res.status = 204;  // No Content
        return;
    }
    
    // 4. Assign Job to Engine
    assign_job_to_engine(pending_job, selected_engine);
    save_state_unlocked();
    
    // 5. Return Assigned Job
    std::string job_id = pending_job["job_id"].get<std::string>();
    set_json_response(res, jobs_db[job_id], 200);
}
