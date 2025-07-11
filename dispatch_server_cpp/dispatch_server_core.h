#ifndef DISPATCH_SERVER_CORE_H
#define DISPATCH_SERVER_CORE_H

#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include "nlohmann/json.hpp"
#include "httplib.h"
#include "repositories.h"
#include "api_middleware.h"

// Legacy global state (for backward compatibility with tests)
extern nlohmann::json jobs_db;
extern nlohmann::json engines_db;
extern std::mutex state_mutex;

// Persistent storage file
extern std::string PERSISTENT_STORAGE_FILE;

// Global flag and counter for mocking save_state
extern bool mock_save_state_enabled;
extern int save_state_call_count;

// Global flag and data for mocking load_state
extern bool mock_load_state_enabled;
extern nlohmann::json mock_load_state_data;

// Job and Engine domain classes
struct Job {
    std::string id;
    std::string status;
    std::string source_url;
    std::string output_url;
    std::string assigned_engine;
    std::string codec;
    double job_size;
    int max_retries;
    int retries;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    int priority = 0; // 0=normal, 1=high, 2=urgent
    
    nlohmann::json to_json() const;
    static Job from_json(const nlohmann::json& j);
};

struct Engine {
    std::string id;
    std::string hostname;
    std::string status;
    double benchmark_time;
    bool can_stream;
    int storage_capacity_gb;
    std::chrono::system_clock::time_point last_heartbeat;
    std::string current_job_id;
    
    nlohmann::json to_json() const;
    static Engine from_json(const nlohmann::json& j);
};

class DispatchServer {
public:
    // Constructor with dependency injection
    DispatchServer(std::unique_ptr<IJobRepository> job_repo, 
                   std::unique_ptr<IEngineRepository> engine_repo);
    
    // Default constructor (for backward compatibility)
    DispatchServer();
    
    void start(int port, bool block = true);
    void stop();
    httplib::Server* getServer();
    void set_api_key(const std::string& key);
    
    // For testing
    IJobRepository* get_job_repository() { return job_repo_.get(); }
    IEngineRepository* get_engine_repository() { return engine_repo_.get(); }

private:
    httplib::Server svr;
    std::thread server_thread;
    std::string api_key_;
    
    // Thread-safe background processing
    std::atomic<bool> shutdown_requested_{false};
    std::thread background_worker_;
    std::future<void> state_save_future_;
    
    // Injected dependencies
    std::unique_ptr<IJobRepository> job_repo_;
    std::unique_ptr<IEngineRepository> engine_repo_;
    
    // Whether to use legacy global state or repositories
    bool use_legacy_storage_;
    
    void setup_endpoints();
    void setup_job_endpoints();
    void setup_engine_endpoints();
    void setup_system_endpoints();
    
    // Background processing
    void background_worker();
    void cleanup_stale_engines();
    void handle_job_timeouts();
    void async_save_state();
    
    // Thread-safe job operations
    Job* find_next_pending_job();
    Engine* find_best_engine_for_job(const Job& job);
};

// Function declarations (for backward compatibility)
void load_state();
void save_state();
void save_state_unlocked();
void async_save_state();
std::string generate_uuid();
void setup_endpoints(httplib::Server &svr, const std::string& api_key);
DispatchServer* run_dispatch_server(int argc, char* argv[], DispatchServer* server_instance);
DispatchServer* run_dispatch_server(DispatchServer* server_instance);

#endif // DISPATCH_SERVER_CORE_H