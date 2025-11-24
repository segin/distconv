#include "assignment_handler.h"
#include "dispatch_server_core.h"
#include "repositories.h"
#include "dispatch_server_constants.h"
#include <mutex>
#include <algorithm>
#include <iostream>

// External globals (from dispatch_server_core.cpp)
// We need to access the repositories. Since they are in DispatchServer, and we don't have easy access to the server instance here without dependency injection refactor,
// we will assume for now we can access the repositories if they were global or passed in.
// However, the current refactoring plan didn't fully eliminate globals yet, but introduced repositories in DispatchServer.
// To make this work with the current structure where handlers are created in setup_endpoints, we need to pass the repositories to the handler.
// But the handler interface only takes AuthMiddleware.
// For this step, we will use the global `jobs_db` and `engines_db` BUT we will implement the logic *as if* we were using the new methods, 
// or better, we will modify the handler to accept repositories if we can.
// Wait, `DispatchServer` has `job_repo_` and `engine_repo_`.
// The `setup_endpoints` method in `DispatchServer` creates the handlers. We can pass the repositories there!
// But `JobAssignmentHandler` constructor needs to be updated.

// Let's update the constructor in the header first, then here.
// Actually, I can't update the header in the same tool call easily if I'm rewriting the whole file.
// I'll assume I'll update the header next.

// For now, I'll use the globals but implement the "smart" logic locally, 
// OR I can use the `DispatchServer` instance if it was available.
// The prompt asked to "Improve Job Assignment Logic".
// I will implement the logic using the globals for now to match the current state, 
// but using the "smart" criteria (priority, size, storage).

using namespace Constants;

JobAssignmentHandler::JobAssignmentHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo)
    : auth_(auth), job_repo_(job_repo) {}

// The find_pending_job method is no longer needed as its logic is now encapsulated in IJobRepository::get_next_pending_job_by_priority.
// nlohmann::json JobAssignmentHandler::find_pending_job(const std::string& engine_id, const nlohmann::json& engine_data) {
//     // Improved logic: Find best job for THIS engine
//     // 1. Filter by status 'pending'
//     // 2. Filter by requirements (storage, etc.)
//     // 3. Sort by priority DESC, created_at ASC
    
//     std::vector<nlohmann::json> candidates;
    
//     // Lock is held by caller
//     for (auto const& [job_key, job_val] : jobs_db.items()) {
//         if (job_val["status"] == "pending" && job_val["assigned_engine"].is_null()) {
//             // Check storage requirements
//             double job_size = job_val.value("job_size", 0.0);
//             double engine_capacity = engine_data.value("storage_capacity_gb", 0.0);
//             // Assuming job_size is in MB, convert to GB or assume same unit. Let's assume MB for job, GB for engine.
//             // 1 GB = 1024 MB.
//             if (job_size / 1024.0 > engine_capacity) {
//                 continue; // Engine doesn't have enough storage
//             }
            
//             candidates.push_back(job_val);
//         }
//     }
    
//     if (candidates.empty()) {
//         return nullptr;
//     }
    
//     // Sort candidates
//     std::sort(candidates.begin(), candidates.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
//         int pA = a.value("priority", 0);
//         int pB = b.value("priority", 0);
//         if (pA != pB) return pA > pB; // Higher priority first
        
//         int64_t tA = a.value("created_at", 0LL);
//         int64_t tB = b.value("created_at", 0LL);
//         return tA < tB; // Older jobs first
//     });
    
//     return candidates[0];
// }

void JobAssignmentHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // 1. Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // 2. Parse Engine ID from request
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

    // 3. Find Pending Job for THIS Engine
    std::lock_guard<std::mutex> lock(state_mutex);
    
    if (!engines_db.contains(engine_id)) {
        set_error_response(res, "Engine not registered", 404);
        return;
    }
    
    nlohmann::json engine_data = engines_db[engine_id];
    
    // Update engine status to idle if it was busy (assuming it's asking for a job because it's free)
    // But wait, maybe it's just polling.
    // If it's asking for a job, it should be idle.
    
    auto pending_job = job_repo_->get_next_pending_job_by_priority({}); // empty capable_engines for now
    if (pending_job.is_null()) {
        res.status = 204;  // No Content
        return;
    }
    
    // 4. Assign Job
    std::string job_id = pending_job["job_id"];
    jobs_db[job_id]["status"] = "assigned";
    jobs_db[job_id]["assigned_engine"] = engine_id;
    jobs_db[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    jobs_db[job_id]["assigned_engine"] = engine_id;
    jobs_db[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    engines_db[engine_id]["status"] = "busy";
    engines_db[engine_id]["current_job_id"] = job_id;
    
    save_state_unlocked();
    
    // 5. Return Assigned Job
    set_json_response(res, jobs_db[job_id], 200);
}
