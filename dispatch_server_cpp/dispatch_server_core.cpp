#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <future>
#include <uuid/uuid.h>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "dispatch_server_core.h"
#include "dispatch_server_constants.h"
#include "request_handlers.h"
#include "job_handlers.h"
#include "engine_handlers.h"
#include "job_action_handlers.h"
#include "assignment_handler.h"
#include "job_update_handler.h"
#include "storage_pool_handler.h"
#include "api_middleware.h"
#include "enhanced_endpoints.h"

using namespace Constants;  // For constants


// In-memory storage for jobs and engines (for now)
// In a real application, this would be a database
nlohmann::json jobs_db = nlohmann::json::object();
nlohmann::json engines_db = nlohmann::json::object();
std::mutex state_mutex; // Single mutex for all shared state
std::mutex jobs_mutex; // Dedicated mutex for jobs
std::mutex engines_mutex; // Dedicated mutex for engines

// Persistent storage for jobs and engines
std::string PERSISTENT_STORAGE_FILE = "dispatch_server_state.json";

bool mock_save_state_enabled = false;
int save_state_call_count = 0;

// Forward declaration for save_state_unlocked
void save_state_unlocked();

bool mock_load_state_enabled = false;
nlohmann::json mock_load_state_data = nlohmann::json::object();

// Job and Engine struct implementations
nlohmann::json Job::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["status"] = status;
    j["source_url"] = source_url;
    j["output_url"] = output_url;
    j["assigned_engine"] = assigned_engine;
    j["codec"] = codec;
    j["job_size"] = job_size;
    j["max_retries"] = max_retries;
    j["retries"] = retries;
    j["priority"] = priority;
    j["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(created_at.time_since_epoch()).count();
    j["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(updated_at.time_since_epoch()).count();
    return j;
}

Job Job::from_json(const nlohmann::json& j) {
    Job job;
    job.id = j.value("id", "");
    job.status = j.value("status", "pending");
    job.source_url = j.value("source_url", "");
    job.output_url = j.value("output_url", "");
    job.assigned_engine = j.value("assigned_engine", "");
    job.codec = j.value("codec", "");
    job.job_size = j.value("job_size", 0.0);
    job.max_retries = j.value("max_retries", 3);
    job.retries = j.value("retries", 0);
    job.priority = j.value("priority", 0);
    
    if (j.contains("created_at")) {
        job.created_at = std::chrono::system_clock::time_point{std::chrono::milliseconds{j["created_at"]}};
    } else {
        job.created_at = std::chrono::system_clock::now();
    }
    
    if (j.contains("updated_at")) {
        job.updated_at = std::chrono::system_clock::time_point{std::chrono::milliseconds{j["updated_at"]}};
    } else {
        job.updated_at = std::chrono::system_clock::now();
    }
    
    return job;
}

nlohmann::json Engine::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["hostname"] = hostname;
    j["status"] = status;
    j["benchmark_time"] = benchmark_time;
    j["can_stream"] = can_stream;
    j["storage_capacity_gb"] = storage_capacity_gb;
    j["current_job_id"] = current_job_id;
    j["last_heartbeat"] = std::chrono::duration_cast<std::chrono::milliseconds>(last_heartbeat.time_since_epoch()).count();
    return j;
}

Engine Engine::from_json(const nlohmann::json& j) {
    Engine engine;
    engine.id = j.value("id", "");
    engine.hostname = j.value("hostname", "");
    engine.status = j.value("status", "idle");
    engine.benchmark_time = j.value("benchmark_time", 0.0);
    engine.can_stream = j.value("can_stream", false);
    engine.storage_capacity_gb = j.value("storage_capacity_gb", 0);
    engine.current_job_id = j.value("current_job_id", "");
    
    if (j.contains("last_heartbeat")) {
        engine.last_heartbeat = std::chrono::system_clock::time_point{std::chrono::milliseconds{j["last_heartbeat"]}};
    } else {
        engine.last_heartbeat = std::chrono::system_clock::now();
    }
    
    return engine;
}

// Utility function to generate UUID for job IDs
std::string generate_uuid() {
    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
}

void load_state() {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::cout << "[load_state] Attempting to load state." << std::endl;
    jobs_db.clear();
    engines_db.clear();

    if (mock_load_state_enabled) {
        std::cout << "[load_state] Mocking enabled. Loading mocked data." << std::endl;
        if (mock_load_state_data.contains("jobs")) {
            jobs_db = mock_load_state_data["jobs"];
        }
        if (mock_load_state_data.contains("engines")) {
            engines_db = mock_load_state_data["engines"];
        }
        std::cout << "[load_state] Loaded mocked state: jobs=" << jobs_db.size() << ", engines=" << engines_db.size() << std::endl;
        return;
    }

    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    if (ifs.is_open()) {
        std::cout << "[load_state] Persistent storage file opened: " << PERSISTENT_STORAGE_FILE << std::endl;
        try {
            nlohmann::json state = nlohmann::json::parse(ifs);
            if (state.contains("jobs")) {
                jobs_db = state["jobs"];
            }
            if (state.contains("engines")) {
                engines_db = state["engines"];
            }
            std::cout << "[load_state] Successfully loaded state: jobs=" << jobs_db.size() << ", engines=" << engines_db.size() << std::endl;
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "[load_state] Error parsing persistent state file: " << e.what() << std::endl;
        }
        ifs.close();
    } else {
        std::cout << "[load_state] Persistent storage file not found or could not be opened: " << PERSISTENT_STORAGE_FILE << ". Starting with empty state." << std::endl;
    }
}

// This version of save_state assumes the caller already holds the lock
void save_state_unlocked() {
    std::ofstream ofs(PERSISTENT_STORAGE_FILE);
    if (ofs.is_open()) {
        nlohmann::json state_to_save;
        state_to_save["jobs"] = jobs_db;
        state_to_save["engines"] = engines_db;
        ofs << state_to_save.dump(4) << std::endl;
        ofs.close();
        std::cout << "Saved state." << std::endl;
    }
}

void save_state() {
    if (mock_save_state_enabled) {
        save_state_call_count++;
        return;
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    save_state_unlocked();
}

// Asynchronous save state function
void async_save_state() {
    if (mock_save_state_enabled) {
        save_state_call_count++;
        return;
    }
    
    // Create a copy of the state under lock, then save asynchronously
    nlohmann::json state_copy;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        state_copy["jobs"] = jobs_db;
        state_copy["engines"] = engines_db;
    }
    
    // Use a temporary file for atomic write
    std::string temp_file = PERSISTENT_STORAGE_FILE + ".tmp";
    std::ofstream ofs(temp_file);
    if (ofs.is_open()) {
        ofs << state_copy.dump(4) << std::endl;
        ofs.close();
        
        // Atomic rename
        if (std::rename(temp_file.c_str(), PERSISTENT_STORAGE_FILE.c_str()) == 0) {
            std::cout << "Saved state asynchronously." << std::endl;
        } else {
            std::cerr << "Failed to atomically save state." << std::endl;
            std::remove(temp_file.c_str()); // Clean up temp file
        }
    } else {
        std::cerr << "Failed to open temporary file for state saving." << std::endl;
    }
}

void setup_endpoints(httplib::Server &svr, const std::string& api_key); // Forward declaration

// Background processing methods
void DispatchServer::background_worker() {
    while (!shutdown_requested_.load()) {
        try {
            cleanup_stale_engines();
            handle_job_timeouts();
            requeue_failed_jobs();
            expire_pending_jobs();
            
            // Sleep between cleanup iterations, but wake up immediately on shutdown
            std::unique_lock<std::mutex> lock(shutdown_mutex_);
            shutdown_cv_.wait_for(lock, BACKGROUND_WORKER_INTERVAL, [this] {
                return shutdown_requested_.load();
            });
        } catch (const std::exception& e) {
            std::cerr << "Background worker error: " << e.what() << std::endl;
        }
    }
}

void DispatchServer::cleanup_stale_engines() {
    // Use repository if available (preferred), otherwise legacy
    if (!use_legacy_storage_ && engine_repo_) {
        auto engines = engine_repo_->get_all_engines();
        auto now = std::chrono::system_clock::now();
        
        for (const auto& engine : engines) {
            if (engine.contains("last_heartbeat")) {
                auto last_heartbeat = std::chrono::system_clock::time_point{
                    std::chrono::milliseconds{engine["last_heartbeat"]}
                };
                auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - last_heartbeat);
                
                if (duration > ENGINE_HEARTBEAT_TIMEOUT) {
                    std::string engine_id = engine["engine_id"];
                    std::cout << "Removing stale engine: " << engine_id << std::endl;
                    
                    // Check if this engine had a job assigned
                    if (engine.contains("current_job_id") && !engine["current_job_id"].get<std::string>().empty()) {
                        std::string job_id = engine["current_job_id"];
                        std::cout << "Re-queuing job " << job_id << " from stale engine " << engine_id << std::endl;
                        
                        // Re-queue the job
                        if (job_repo_->job_exists(job_id)) {
                            nlohmann::json job = job_repo_->get_job(job_id);
                            // Only re-queue if it was still marked as assigned/processing
                            if (job["status"] == "assigned" || job["status"] == "processing") {
                                job["status"] = "pending";
                                job["assigned_engine"] = nullptr;
                                job["retries"] = job.value("retries", 0) + 1; // Count as a retry since it failed to complete
                                job_repo_->save_job(job_id, job);
                            }
                        }
                    }
                    
                    engine_repo_->remove_engine(engine_id);
                }
            }
        }
        return;
    }

    if (use_legacy_storage_) {
        std::lock_guard<std::mutex> lock(state_mutex);
        auto now = std::chrono::system_clock::now();
        
        for (auto it = engines_db.begin(); it != engines_db.end();) {
            if (it->contains("last_heartbeat")) {
                auto last_heartbeat = std::chrono::system_clock::time_point{
                    std::chrono::milliseconds{it.value()["last_heartbeat"]}
                };
                auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - last_heartbeat);
                
                if (duration > ENGINE_HEARTBEAT_TIMEOUT) {
                    std::cout << "Removing stale engine: " << it.key() << std::endl;
                    
                    // Legacy: Re-queue job if assigned
                    if (it.value().contains("current_job_id") && !it.value()["current_job_id"].get<std::string>().empty()) {
                         std::string job_id = it.value()["current_job_id"];
                         if (jobs_db.contains(job_id)) {
                             jobs_db[job_id]["status"] = "pending";
                             jobs_db[job_id]["assigned_engine"] = nullptr;
                             jobs_db[job_id]["retries"] = jobs_db[job_id].value("retries", 0) + 1;
                         }
                    }

                    it = engines_db.erase(it);
                    async_save_state();
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }
}

void DispatchServer::handle_job_timeouts() {
    // Use repository if available
    if (!use_legacy_storage_ && job_repo_) {
        // We need to find assigned jobs that haven't been updated in a while.
        // Since we don't have a direct query for this in the interface yet, we might need to iterate or add a method.
        // For now, let's iterate all jobs (inefficient but works for now) or rely on the engine heartbeat timeout to catch crashes.
        // Actually, job timeout is different from engine timeout. A job might be stuck processing.
        // Let's iterate for now, but in a real app we'd add `get_stuck_jobs` to the repo.
        
        auto jobs = job_repo_->get_all_jobs();
        auto now = std::chrono::system_clock::now();
        
        for (auto& job : jobs) {
            if (job["status"] == "assigned" || job["status"] == "processing") {
                if (job.contains("updated_at")) {
                    auto updated_at = std::chrono::system_clock::time_point{
                        std::chrono::milliseconds{job["updated_at"]}
                    };
                    auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - updated_at);
                    
                    if (duration > JOB_TIMEOUT) {
                        std::string job_id = job["job_id"];
                        std::cout << "Job " << job_id << " timed out, marking as failed" << std::endl;
                        
                        // Mark as failed/retry
                        int retries = job.value("retries", 0);
                        int max_retries = job.value("max_retries", 3);
                        
                        if (retries < max_retries) {
                            // Backoff: 2^retries * 30 seconds
                            int backoff_seconds = (1 << retries) * 30;
                            auto retry_after = now + std::chrono::seconds(backoff_seconds);
                            int64_t retry_after_ms = std::chrono::duration_cast<std::chrono::milliseconds>(retry_after.time_since_epoch()).count();
                            
                            job["status"] = "failed_retry";
                            job["retry_after"] = retry_after_ms;
                            job["retries"] = retries + 1;
                        } else {
                            job["status"] = "failed_permanently";
                            job["error_message"] = "Job timed out and exceeded max retries";
                        }
                        
                        job["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                        
                        // Free up the engine
                        if (job.contains("assigned_engine") && !job["assigned_engine"].is_null()) {
                            std::string engine_id = job["assigned_engine"];
                            if (engine_repo_->engine_exists(engine_id)) {
                                nlohmann::json engine = engine_repo_->get_engine(engine_id);
                                engine["status"] = "idle";
                                engine["current_job_id"] = "";
                                engine_repo_->save_engine(engine_id, engine);
                            }
                        }
                        
                        job_repo_->save_job(job_id, job);
                    }
                }
            }
        }
        return;
    }

    if (use_legacy_storage_) {
        std::lock_guard<std::mutex> lock(state_mutex);
        auto now = std::chrono::system_clock::now();
        
        for (auto& [job_id, job_data] : jobs_db.items()) {
            if (job_data["status"] == "assigned" && job_data.contains("updated_at")) {
                auto updated_at = std::chrono::system_clock::time_point{
                    std::chrono::milliseconds{job_data["updated_at"]}
                };
                auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - updated_at);
                
                if (duration > JOB_TIMEOUT) {
                    std::cout << "Job " << job_id << " timed out, marking as failed" << std::endl;
                    job_data["status"] = "failed";
                    job_data["retries"] = job_data.value("retries", 0) + 1;
                    job_data["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                    
                    // Free up the engine
                    if (job_data.contains("assigned_engine") && !job_data["assigned_engine"].is_null()) {
                        std::string engine_id = job_data["assigned_engine"];
                        if (engines_db.contains(engine_id)) {
                            engines_db[engine_id]["status"] = "idle";
                            engines_db[engine_id]["current_job_id"] = "";
                        }
                    }
                    
                    async_save_state();
                }
            }
        }
    }
}

void DispatchServer::requeue_failed_jobs() {
    if (!use_legacy_storage_ && job_repo_) {
        // Iterate jobs to find 'failed_retry' ones that are ready
        // Ideally this should be a repo method `get_ready_to_retry_jobs()`
        auto jobs = job_repo_->get_all_jobs();
        auto now = std::chrono::system_clock::now();
        int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        for (auto& job : jobs) {
            if (job.value("status", "") == "failed_retry") {
                int64_t retry_after = job.value("retry_after", 0LL);
                if (now_ms >= retry_after) {
                    std::cout << "Re-queuing job " << job["job_id"] << " for retry." << std::endl;
                    job["status"] = "pending";
                    job["updated_at"] = now_ms;
                    job_repo_->save_job(job["job_id"], job);
                }
            }
        }
    }
}

void DispatchServer::expire_pending_jobs() {
    if (!use_legacy_storage_ && job_repo_) {
        // 24 hours expiration for pending jobs
        int64_t timeout_seconds = 24 * 3600; 
        auto stale_job_ids = job_repo_->get_stale_pending_jobs(timeout_seconds);
        
        for (const auto& job_id : stale_job_ids) {
            std::cout << "Expiring stale pending job " << job_id << std::endl;
            nlohmann::json job = job_repo_->get_job(job_id);
            job["status"] = "expired";
            job["error_message"] = "Job expired after being pending for too long";
            job_repo_->save_job(job_id, job);
        }
    }
}

void DispatchServer::async_save_state() {
    // Launch async save if previous one is complete
    if (state_save_future_.valid() && state_save_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        state_save_future_.get(); // Clear the future
    }
    
    if (!state_save_future_.valid()) {
        state_save_future_ = std::async(std::launch::async, ::async_save_state);
    }
}

// Improved job scheduling methods
Job* DispatchServer::find_next_pending_job() {
    // This would use the repository in non-legacy mode
    // For now, working with legacy storage
    return nullptr;
}

Engine* DispatchServer::find_best_engine_for_job(const Job& job) {
    // This would use the repository in non-legacy mode
    // For now, working with legacy storage
    return nullptr;
}

// Constructor with dependency injection
// Constructor with dependency injection
DispatchServer::DispatchServer(std::shared_ptr<IJobRepository> job_repo, 
                               std::shared_ptr<IEngineRepository> engine_repo,
                               const std::string& api_key) 
    : job_repo_(job_repo), 
      engine_repo_(engine_repo),
      use_legacy_storage_(false),
      api_key_(api_key) {
    // Endpoints will be set up when set_api_key is called or immediately if key is provided
    if (!api_key_.empty()) {
        setup_endpoints();
    }
}

// Default constructor (for backward compatibility)
DispatchServer::DispatchServer() 
    : job_repo_(nullptr), 
      engine_repo_(nullptr),
      use_legacy_storage_(true) {
    // Endpoints will be set up when set_api_key is called
}

void DispatchServer::set_api_key(const std::string& key) {
    api_key_ = key;
    // Re-setup endpoints with the new API key
    if (use_legacy_storage_) {
        ::setup_endpoints(svr, api_key_);
    } else {
        setup_endpoints();
    }
}



void DispatchServer::setup_storage_endpoints() {
    // Create storage repository (in-memory for now)
    static auto storage_repo = std::make_shared<InMemoryStorageRepository>();
    
    // POST /storage_pools/ - Create storage pool
    auto storage_create_handler = std::make_shared<StoragePoolCreateHandler>(std::make_shared<AuthMiddleware>(api_key_), storage_repo);
    svr.Post("/storage_pools/", [storage_create_handler](const httplib::Request& req, httplib::Response& res) {
        storage_create_handler->handle(req, res);
    });

    // GET /storage_pools/ - List storage pools
    auto storage_list_handler = std::make_shared<StoragePoolListHandler>(std::make_shared<AuthMiddleware>(api_key_), storage_repo);
    svr.Get("/storage_pools/", [storage_list_handler](const httplib::Request& req, httplib::Response& res) {
        storage_list_handler->handle(req, res);
    });

    // PUT /storage_pools/{id} - Update storage pool
    auto storage_update_handler = std::make_shared<StoragePoolUpdateHandler>(std::make_shared<AuthMiddleware>(api_key_), storage_repo);
    svr.Put(R"(/storage_pools/(.+))", [storage_update_handler](const httplib::Request& req, httplib::Response& res) {
        storage_update_handler->handle(req, res);
    });

    // DELETE /storage_pools/{id} - Delete storage pool
    auto storage_delete_handler = std::make_shared<StoragePoolDeleteHandler>(std::make_shared<AuthMiddleware>(api_key_), storage_repo);
    svr.Delete(R"(/storage_pools/(.+))", [storage_delete_handler](const httplib::Request& req, httplib::Response& res) {
        storage_delete_handler->handle(req, res);
    });
}

void DispatchServer::start(int port, bool block) {
    // Start background worker thread
    shutdown_requested_.store(false);
    background_worker_ = std::thread(&DispatchServer::background_worker, this);
    
    if (block) {
        svr.listen("0.0.0.0", port);
    } else {
        server_thread = std::thread([this, port]() {
            this->svr.listen("0.0.0.0", port);
        });
    }
}

void DispatchServer::stop() {
    // Stop background worker
    shutdown_requested_.store(true);
    shutdown_cv_.notify_all();  // Wake up the background worker immediately
    if (background_worker_.joinable()) {
        background_worker_.join();
    }
    
    // Wait for any pending async save to complete
    if (state_save_future_.valid()) {
        state_save_future_.wait();
    }
    
    svr.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
    save_state();
}

httplib::Server* DispatchServer::getServer() {
    return &svr;
}

void setup_endpoints(httplib::Server &svr, const std::string& api_key) {
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });
    
    // Create handlers - BEFORE enhanced endpoints to override legacy behavior
    auto auth = std::make_shared<AuthMiddleware>(api_key);
    auto job_submission_handler = std::make_shared<JobSubmissionHandler>(auth);
    
    auto job_status_handler = std::make_shared<JobStatusHandler>(auth);

    // Endpoint to submit a new transcoding job - Now using handler!
    // This must be registered BEFORE enhanced endpoints to override the legacy endpoint
    svr.Post("/jobs/", [job_submission_handler](const httplib::Request& req, httplib::Response& res) {
        job_submission_handler->handle(req, res);
    });

    // Set up enhanced API v1 endpoints
    setup_enhanced_job_endpoints(svr, api_key);
    setup_enhanced_system_endpoints(svr, api_key);

    // Endpoint to get job status
    svr.Get(R"(/jobs/(.+))", [job_status_handler](const httplib::Request& req, httplib::Response& res) {
        job_status_handler->handle(req, res);
    });

    auto job_list_handler = std::make_shared<JobListHandler>(auth);

    // Endpoint to list all jobs
    svr.Get("/jobs/", [job_list_handler](const httplib::Request& req, httplib::Response& res) {
        job_list_handler->handle(req, res);
    });

    // Endpoint to list all engines
    auto engine_list_handler = std::make_shared<EngineListHandler>(auth);
    svr.Get("/engines/", [engine_list_handler](const httplib::Request& req, httplib::Response& res) {
        engine_list_handler->handle(req, res);
    });

    // Endpoint for engine heartbeat
    auto engine_heartbeat_handler = std::make_shared<EngineHeartbeatHandler>(auth);
    svr.Post("/engines/heartbeat", [engine_heartbeat_handler](const httplib::Request& req, httplib::Response& res) {
        engine_heartbeat_handler->handle(req, res);
    });


    // Endpoint for benchmark results
    auto engine_benchmark_handler = std::make_shared<EngineBenchmarkHandler>(auth);
    svr.Post("/engines/benchmark_result", [engine_benchmark_handler](const httplib::Request& req, httplib::Response& res) {
        engine_benchmark_handler->handle(req, res);
    });


    // Endpoint to complete a job
    auto job_completion_handler = std::make_shared<JobCompletionHandler>(auth);
    svr.Post(R"(/jobs/([a-fA-F0-9\\-]{36})/complete)", [job_completion_handler](const httplib::Request& req, httplib::Response& res) {
        job_completion_handler->handle(req, res);
    });

    // Endpoint to fail a job
    auto job_failure_handler = std::make_shared<JobFailureHandler>(auth);
    svr.Post(R"(/jobs/([a-fA-F0-9\\-]{36})/fail)", [job_failure_handler](const httplib::Request& req, httplib::Response& res) {
        job_failure_handler->handle(req, res);
    });



    // Endpoint to assign a job (for engines to poll)
    // Note: This is legacy code, but we need to update it to use the new constructor
    // For now, we need to get the job_repo from global variables
    extern nlohmann::json jobs_db;
    extern std::mutex state_mutex;
    auto legacy_job_repo = std::make_shared<InMemoryJobRepository>();
    // Copy jobs_db into the repository
    for (const auto& [id, job] : jobs_db.items()) {
        legacy_job_repo->save_job(id, job);
    }
    auto job_assignment_handler = std::make_shared<JobAssignmentHandler>(auth, legacy_job_repo);
    svr.Post("/assign_job/", [job_assignment_handler](const httplib::Request& req, httplib::Response& res) {
        job_assignment_handler->handle(req, res);
    });



    // Placeholder for storage pool configuration (to be implemented later)
    svr.Get("/storage_pools/", [api_key](const httplib::Request& req, httplib::Response& res) {
        res.set_content("Storage pool configuration to be implemented.", "text/plain");
    });
}

DispatchServer* run_dispatch_server(int argc, char* argv[], DispatchServer* server_instance) {
    std::string api_key = "";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--api-key") {
            if (i + 1 < argc && argv[i+1][0] != '-') { // Check if next argument exists and is not another flag
                api_key = argv[++i];
            } else {
                // Handle case where --api-key is the last argument or followed by another flag
                api_key = ""; // Explicitly set to empty if no valid value is found
            }
        }
    }

    if (server_instance) {
        server_instance->set_api_key(api_key);
        std::cout << "Dispatch Server listening on port 8080" << std::endl;
        server_instance->start(8080, false); // Start in non-blocking mode
    }

    return server_instance;
}

DispatchServer* run_dispatch_server(DispatchServer* server_instance) {
    if (server_instance) {
        server_instance->set_api_key(""); // No API key when called without args
        std::cout << "Dispatch Server listening on port 8080" << std::endl;
        server_instance->start(8080, false); // Start in non-blocking mode
    }
    return server_instance;
}

// New dependency-injected setup_endpoints method
void DispatchServer::setup_endpoints() {
    // Note: Cannot reassign httplib::Server, so we'll just set up endpoints
    // The server endpoints will be overwritten by the new ones
    
    setup_system_endpoints();
    setup_job_endpoints();
    setup_engine_endpoints();
    setup_storage_endpoints();
}

void DispatchServer::setup_system_endpoints() {
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });

    // Health check endpoint
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json health_response;
        health_response["status"] = "healthy";
        health_response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        res.set_content(health_response.dump(), "application/json");
    });
}

void DispatchServer::setup_job_endpoints() {
    // POST /jobs/ - Submit a new job
    svr.Post("/jobs/", ApiMiddleware::with_api_key_validation(api_key_, 
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                nlohmann::json request_json = nlohmann::json::parse(req.body);

                // Validate required fields
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

                // Validate optional fields
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

                // Generate unique job ID
                static std::atomic<int> job_counter{0};
                auto now = std::chrono::system_clock::now().time_since_epoch();
                auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
                std::string job_id = std::to_string(microseconds) + "_" + std::to_string(job_counter.fetch_add(1));

                // Create job object
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

                // Save job
                job_repo_->save_job(job_id, job);

                res.status = 201;
                res.set_content(job.dump(), "application/json");
            } catch (const nlohmann::json::parse_error& e) {
                res.status = 400;
                res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content("Server error: " + std::string(e.what()), "text/plain");
            }
        }));

    // GET /jobs/{job_id} - Get job status
    svr.Get(R"(/jobs/(.+))", ApiMiddleware::with_api_key_validation(api_key_, 
        [this](const httplib::Request& req, httplib::Response& res) {
            std::string job_id = req.matches[1];
            
            if (job_repo_->job_exists(job_id)) {
                nlohmann::json job = job_repo_->get_job(job_id);
                res.set_content(job.dump(), "application/json");
            } else {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
            }
        }));

    // GET /jobs/ - List all jobs
    svr.Get("/jobs/", ApiMiddleware::with_api_key_validation(api_key_, 
        [this](const httplib::Request& req, httplib::Response& res) {
            std::vector<nlohmann::json> jobs = job_repo_->get_all_jobs();
            nlohmann::json response = nlohmann::json::array();
            
            for (const auto& job : jobs) {
                response.push_back(job);
            }
            
            res.set_content(response.dump(), "application/json");
        }));

    // POST /jobs/{job_id}/complete - Mark job as completed
    svr.Post(R"(/jobs/(.+)/complete)", ApiMiddleware::with_api_key_validation(api_key_, 
        [this](const httplib::Request& req, httplib::Response& res) {
            std::string job_id = req.matches[1];
            
            if (!job_repo_->job_exists(job_id)) {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
                return;
            }

            try {
                nlohmann::json request_json = nlohmann::json::parse(req.body);
                
                nlohmann::json job = job_repo_->get_job(job_id);
                job["status"] = "completed";
                job["output_url"] = request_json.value("output_url", "");
                
                job_repo_->save_job(job_id, job);
                
                res.set_content(job.dump(), "application/json");
            } catch (const nlohmann::json::parse_error& e) {
                res.status = 400;
                res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content("Server error: " + std::string(e.what()), "text/plain");
            }
        }));

    // POST /jobs/{job_id}/fail - Mark job as failed
    svr.Post(R"(/jobs/(.+)/fail)", ApiMiddleware::with_api_key_validation(api_key_, 
        [this](const httplib::Request& req, httplib::Response& res) {
            std::string job_id = req.matches[1];
            
            if (!job_repo_->job_exists(job_id)) {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
                return;
            }

            try {
                nlohmann::json request_json = nlohmann::json::parse(req.body);
                
                nlohmann::json job = job_repo_->get_job(job_id);
                job["status"] = "failed";
                job["error_message"] = request_json.value("error_message", "");
                
                job_repo_->save_job(job_id, job);
                
                res.set_content(job.dump(), "application/json");
            } catch (const nlohmann::json::parse_error& e) {
                res.status = 400;
                res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content("Server error: " + std::string(e.what()), "text/plain");
            }
        }));

    // POST /jobs/{job_id}/retry - Manually retry a failed job
    auto job_retry_handler = std::make_shared<JobRetryHandler>(std::make_shared<AuthMiddleware>(api_key_));
    svr.Post(R"(/jobs/(.+)/retry)", [job_retry_handler](const httplib::Request& req, httplib::Response& res) {
        job_retry_handler->handle(req, res);
    });

    // POST /jobs/{job_id}/cancel - Cancel a job
    auto job_cancel_handler = std::make_shared<JobCancelHandler>(std::make_shared<AuthMiddleware>(api_key_));
    svr.Post(R"(/jobs/(.+)/cancel)", [job_cancel_handler](const httplib::Request& req, httplib::Response& res) {
        job_cancel_handler->handle(req, res);
    });



    // New endpoint registrations
    
    // PUT /jobs/{job_id} - Update job parameters
    auto job_update_handler = std::make_shared<JobUpdateHandler>(std::make_shared<AuthMiddleware>(api_key_), job_repo_);
    svr.Put(R"(/jobs/(.+))", [job_update_handler](const httplib::Request& req, httplib::Response& res) {
        job_update_handler->handle(req, res);
    });

    // PUT /jobs/{job_id}/status - Un ified status update
    auto job_unified_status_handler = std::make_shared<JobUnifiedStatusHandler>(std::make_shared<AuthMiddleware>(api_key_), job_repo_);
    svr.Put(R"(/jobs/(.+)/status)", [job_unified_status_handler](const httplib::Request& req, httplib::Response& res) {
        job_unified_status_handler->handle(req, res);
    });

    // POST /jobs/{job_id}/progress - Report job progress
    auto job_progress_handler = std::make_shared<JobProgressHandler>(std::make_shared<AuthMiddleware>(api_key_), job_repo_);
    svr.Post(R"(/jobs/(.+)/progress)", [job_progress_handler](const httplib::Request& req, httplib::Response& res) {
        job_progress_handler->handle(req, res);
    });

    // GET /engines/{engine_id}/jobs - Get jobs for a specific engine
    auto engine_jobs_handler = std::make_shared<EngineJobsHandler>(std::make_shared<AuthMiddleware>(api_key_), job_repo_);
    svr.Get(R"(/engines/(.+)/jobs)", [engine_jobs_handler](const httplib::Request& req, httplib::Response& res) {
        engine_jobs_handler->handle(req, res);
    });
}

void DispatchServer::setup_engine_endpoints() {
    // POST /engines/heartbeat - Engine registration/heartbeat
    svr.Post("/engines/heartbeat", ApiMiddleware::with_api_key_validation(api_key_, 
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                nlohmann::json engine_data = nlohmann::json::parse(req.body);
                
                if (!engine_data.contains("engine_id") || !engine_data["engine_id"].is_string()) {
                    res.status = 400;
                    res.set_content("Bad Request: 'engine_id' is missing or not a string.", "text/plain");
                    return;
                }
                
                std::string engine_id = engine_data["engine_id"];
                
                // Add timestamp
                engine_data["last_heartbeat"] = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                
                engine_repo_->save_engine(engine_id, engine_data);
                
                res.set_content("OK", "text/plain");
            } catch (const nlohmann::json::parse_error& e) {
                res.status = 400;
                res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content("Server error: " + std::string(e.what()), "text/plain");
            }
        }));

    // GET /engines/ - List all engines
    svr.Get("/engines/", ApiMiddleware::with_api_key_validation(api_key_, 
        [this](const httplib::Request& req, httplib::Response& res) {
            std::vector<nlohmann::json> engines = engine_repo_->get_all_engines();
            nlohmann::json response = nlohmann::json::array();
            
            for (const auto& engine : engines) {
                response.push_back(engine);
            }
            
            res.set_content(response.dump(), "application/json");
        }));

    // POST /assign_job/ - Assign job to engine
    svr.Post("/assign_job/", ApiMiddleware::with_api_key_validation(api_key_, 
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                nlohmann::json request_json = nlohmann::json::parse(req.body);
                
                if (!request_json.contains("engine_id") || !request_json["engine_id"].is_string()) {
                    res.status = 400;
                    res.set_content("Bad Request: 'engine_id' is missing or not a string.", "text/plain");
                    return;
                }
                
                std::string engine_id = request_json["engine_id"];
                
                // Find a pending job
                std::vector<nlohmann::json> jobs = job_repo_->get_all_jobs();
                nlohmann::json selected_job;
                
                for (const auto& job : jobs) {
                    if (job["status"] == "pending") {
                        selected_job = job;
                        break;
                    }
                }
                
                if (selected_job.is_null()) {
                    res.status = 204; // No Content
                    return;
                }
                
                // Assign the job
                selected_job["status"] = "assigned";
                selected_job["assigned_engine"] = engine_id;
                
                job_repo_->save_job(selected_job["job_id"], selected_job);
                
                // Update engine status
                if (engine_repo_->engine_exists(engine_id)) {
                    nlohmann::json engine = engine_repo_->get_engine(engine_id);
                    engine["status"] = "busy";
                    engine_repo_->save_engine(engine_id, engine);
                }
                
                res.set_content(selected_job.dump(), "application/json");
            } catch (const nlohmann::json::parse_error& e) {
                res.status = 400;
                res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content("Server error: " + std::string(e.what()), "text/plain");
            }
        }));
}
