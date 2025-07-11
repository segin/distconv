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
#include "dispatch_server_core.h" // Include the new header

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
            
            // Sleep for 30 seconds before next iteration
            std::this_thread::sleep_for(std::chrono::seconds(30));
        } catch (const std::exception& e) {
            std::cerr << "Background worker error: " << e.what() << std::endl;
        }
    }
}

void DispatchServer::cleanup_stale_engines() {
    if (use_legacy_storage_) {
        std::lock_guard<std::mutex> lock(state_mutex);
        auto now = std::chrono::system_clock::now();
        
        for (auto it = engines_db.begin(); it != engines_db.end();) {
            if (it->contains("last_heartbeat")) {
                auto last_heartbeat = std::chrono::system_clock::time_point{
                    std::chrono::milliseconds{it.value()["last_heartbeat"]}
                };
                auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - last_heartbeat);
                
                if (duration.count() > 5) { // 5 minutes timeout
                    std::cout << "Removing stale engine: " << it.key() << std::endl;
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
    if (use_legacy_storage_) {
        std::lock_guard<std::mutex> lock(state_mutex);
        auto now = std::chrono::system_clock::now();
        
        for (auto& [job_id, job_data] : jobs_db.items()) {
            if (job_data["status"] == "assigned" && job_data.contains("updated_at")) {
                auto updated_at = std::chrono::system_clock::time_point{
                    std::chrono::milliseconds{job_data["updated_at"]}
                };
                auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - updated_at);
                
                if (duration.count() > 30) { // 30 minutes timeout
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
DispatchServer::DispatchServer(std::unique_ptr<IJobRepository> job_repo, 
                               std::unique_ptr<IEngineRepository> engine_repo) 
    : job_repo_(std::move(job_repo)), 
      engine_repo_(std::move(engine_repo)),
      use_legacy_storage_(false) {
    // Endpoints will be set up when set_api_key is called
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

    // Endpoint to submit a new transcoding job
    svr.Post("/jobs/", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (api_key != "") {
            if (req.get_header_value("X-API-Key") == "") {
                res.status = 401;
                res.set_content("Unauthorized: Missing 'X-API-Key' header.", "text/plain");
                return;
            }
            if (req.get_header_value("X-API-Key") != api_key) {
                res.status = 401;
                res.set_content("Unauthorized", "text/plain");
                return;
            }
        }
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);

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
            if (request_json.contains("job_size")) {
                if (!request_json["job_size"].is_number()) {
                    res.status = 400;
                    res.set_content("Bad Request: 'job_size' must be a number.", "text/plain");
                    return;
                }
                if (request_json["job_size"].is_number() && request_json["job_size"].get<double>() < 0) {
                    res.status = 400;
                    res.set_content("Bad Request: 'job_size' must be a non-negative number.", "text/plain");
                    return;
                }
            }
            if (request_json.contains("max_retries")) {
                if (!request_json["max_retries"].is_number_integer()) {
                    res.status = 400;
                    res.set_content("Bad Request: 'max_retries' must be an integer.", "text/plain");
                    return;
                }
                if (request_json["max_retries"].is_number_integer() && request_json["max_retries"].get<int>() < 0) {
                    res.status = 400;
                    res.set_content("Bad Request: 'max_retries' must be a non-negative integer.", "text/plain");
                    return;
                }
            }

            std::string job_id = generate_uuid();
            
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            
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
            job["priority"] = request_json.value("priority", 0); // 0=normal, 1=high, 2=urgent
            job["created_at"] = now_ms;
            job["updated_at"] = now_ms;

            {
                std::lock_guard<std::mutex> lock(state_mutex);
                jobs_db[job_id] = job;
                save_state_unlocked();
            }

            res.set_content(job.dump(), "application/json");
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const nlohmann::json::type_error& e) {
            res.status = 400;
            res.set_content("Bad Request: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint to get job status
    svr.Get(R"(/jobs/(.+))", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (api_key != "") {
            if (req.get_header_value("X-API-Key") == "") {
                res.status = 401;
                res.set_content("Unauthorized: Missing 'X-API-Key' header.", "text/plain");
                return;
            }
            if (req.get_header_value("X-API-Key") != api_key) {
                res.status = 401;
                res.set_content("Unauthorized", "text/plain");
                return;
            }
        }
        std::string job_id = req.matches[1];
        std::lock_guard<std::mutex> lock(state_mutex);
        if (jobs_db.contains(job_id)) {
            res.set_content(jobs_db[job_id].dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content("Job not found", "text/plain");
        }
    });

    // Endpoint to list all jobs
    svr.Get("/jobs/", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (api_key != "") {
            if (req.get_header_value("X-API-Key") == "") {
                res.status = 401;
                res.set_content("Unauthorized: Missing 'X-API-Key' header.", "text/plain");
                return;
            }
            if (req.get_header_value("X-API-Key") != api_key) {
                res.status = 401;
                res.set_content("Unauthorized", "text/plain");
                return;
            }
        }
        nlohmann::json all_jobs = nlohmann::json::array();
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            if (!jobs_db.empty()) {
                for (auto const& [key, val] : jobs_db.items()) {
                    all_jobs.push_back(val);
                }
            }
        }
        res.set_content(all_jobs.dump(), "application/json");
    });

    // Endpoint to list all engines
    svr.Get("/engines/", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (api_key != "") {
            if (req.get_header_value("X-API-Key") == "") {
                res.status = 401;
                res.set_content("Unauthorized: Missing 'X-API-Key' header.", "text/plain");
                return;
            }
            if (req.get_header_value("X-API-Key") != api_key) {
                res.status = 401;
                res.set_content("Unauthorized", "text/plain");
                return;
            }
        }
        nlohmann::json all_engines = nlohmann::json::array();
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            if (!engines_db.empty()) {
                for (auto const& [key, val] : engines_db.items()) {
                    all_engines.push_back(val);
                }
            }
        }
        res.set_content(all_engines.dump(), "application/json");
    });

    // Endpoint for engine heartbeat
    svr.Post("/engines/heartbeat", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (api_key != "") {
            if (req.get_header_value("X-API-Key") == "") {
                res.status = 401;
                res.set_content("Unauthorized: Missing 'X-API-Key' header.", "text/plain");
                return;
            }
            if (req.get_header_value("X-API-Key") != api_key) {
                res.status = 401;
                res.set_content("Unauthorized", "text/plain");
                return;
            }
        }
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (!request_json.contains("engine_id")) {
                res.status = 400;
                res.set_content("Bad Request: 'engine_id' is missing.", "text/plain");
                return;
            }
            if (!request_json["engine_id"].is_string()) {
                res.status = 400;
                res.set_content("Bad Request: 'engine_id' must be a string.", "text/plain");
                return;
            }
            std::string engine_id = request_json["engine_id"];
            if (request_json.contains("storage_capacity_gb")) {
                if (!request_json["storage_capacity_gb"].is_number()) {
                    res.status = 400;
                    res.set_content("Bad Request: 'storage_capacity_gb' must be a number.", "text/plain");
                    return;
                }
                if (request_json["storage_capacity_gb"].is_number() && request_json["storage_capacity_gb"].get<double>() < 0) {
                    res.status = 400;
                    res.set_content("Bad Request: 'storage_capacity_gb' must be a non-negative number.", "text/plain");
                    return;
                }
            }
            if (request_json.contains("streaming_support") && !request_json["streaming_support"].is_boolean()) {
                res.status = 400;
                res.set_content("Bad Request: 'streaming_support' must be a boolean.", "text/plain");
                return;
            }
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                engines_db[engine_id] = request_json;
                save_state_unlocked();
            }
            res.set_content("Heartbeat received from engine " + engine_id, "text/plain");
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint for benchmark results
    svr.Post("/engines/benchmark_result", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (api_key != "") {
            if (req.get_header_value("X-API-Key") == "") {
                res.status = 401;
                res.set_content("Unauthorized: Missing 'X-API-Key' header.", "text/plain");
                return;
            }
            if (req.get_header_value("X-API-Key") != api_key) {
                res.status = 401;
                res.set_content("Unauthorized", "text/plain");
                return;
            }
        }
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            std::string engine_id = request_json["engine_id"];
            if (request_json.contains("benchmark_time")) {
                if (!request_json["benchmark_time"].is_number()) {
                    res.status = 400;
                    res.set_content("Bad Request: 'benchmark_time' must be a number.", "text/plain");
                    return;
                }
                if (request_json["benchmark_time"].is_number() && request_json["benchmark_time"].get<double>() < 0) {
                    res.status = 400;
                    res.set_content("Bad Request: 'benchmark_time' must be a non-negative number.", "text/plain");
                    return;
                }
            }
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                if (engines_db.contains(engine_id)) {
                    engines_db[engine_id]["benchmark_time"] = request_json["benchmark_time"];
                } else {
                    res.status = 404;
                    res.set_content("Engine not found", "text/plain");
                    return;
                }
            }
            res.set_content("Benchmark result received from engine " + engine_id, "text/plain");
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint to complete a job
    svr.Post(R"(/jobs/(\w+)/complete)", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (api_key != "") {
            if (req.get_header_value("X-API-Key") == "") {
                res.status = 401;
                res.set_content("Unauthorized: Missing 'X-API-Key' header.", "text/plain");
                return;
            }
            if (req.get_header_value("X-API-Key") != api_key) {
                res.status = 401;
                res.set_content("Unauthorized", "text/plain");
                return;
            }
        }
        std::string job_id = req.matches[1];
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (!request_json.contains("output_url") || !request_json["output_url"].is_string()) {
                res.status = 400;
                res.set_content("Bad Request: 'output_url' must be a string.", "text/plain");
                return;
            }
            {
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
            }
            res.set_content("Job " + job_id + " marked as completed", "text/plain");
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint to fail a job
    svr.Post(R"(/jobs/(\w+)/fail)", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (api_key != "") {
            if (req.get_header_value("X-API-Key") == "") {
                res.status = 401;
                res.set_content("Unauthorized: Missing 'X-API-Key' header.", "text/plain");
                return;
            }
            if (req.get_header_value("X-API-Key") != api_key) {
                res.status = 401;
                res.set_content("Unauthorized", "text/plain");
                return;
            }
        }
        std::string job_id = req.matches[1];
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (!request_json.contains("error_message")) {
                res.status = 400;
                res.set_content("Bad Request: 'error_message' is missing.", "text/plain");
                return;
            }
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                if (jobs_db.contains(job_id)) {
                    if (jobs_db[job_id]["status"] == "completed" || jobs_db[job_id]["status"] == "failed_permanently") {
                        res.status = 400;
                        res.set_content("Bad Request: Job is already in a final state.", "text/plain");
                        return;
                    }
                    jobs_db[job_id]["retries"] = jobs_db[job_id].value("retries", 0) + 1;
                    if (jobs_db[job_id]["retries"] < jobs_db[job_id].value("max_retries", 3)) {
                        jobs_db[job_id]["status"] = "pending"; // Re-queue
                        jobs_db[job_id]["error_message"] = request_json["error_message"];
                        res.set_content("Job " + job_id + " re-queued", "text/plain");
                    } else {
                        jobs_db[job_id]["status"] = "failed_permanently";
                        jobs_db[job_id]["error_message"] = request_json["error_message"];
                        res.set_content("Job " + job_id + " failed permanently", "text/plain");
                    }
                    save_state_unlocked();
                } else {
                    res.status = 404;
                    res.set_content("Job not found", "text/plain");
                    return;
                }
            }
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint to assign a job (for engines to poll)
    svr.Post("/assign_job/", [api_key](const httplib::Request& req, httplib::Response& res) {
        if (api_key != "") {
            if (req.get_header_value("X-API-Key") == "") {
                res.status = 401;
                res.set_content("Unauthorized: Missing 'X-API-Key' header.", "text/plain");
                return;
            }
            if (req.get_header_value("X-API-Key") != api_key) {
                res.status = 401;
                res.set_content("Unauthorized", "text/plain");
                return;
            }
        }
        
        std::lock_guard<std::mutex> lock(state_mutex);

        nlohmann::json pending_job = nullptr;
        for (auto const& [job_key, job_val] : jobs_db.items()) {
            if (job_val["status"] == "pending" && job_val["assigned_engine"].is_null()) {
                pending_job = job_val;
                break;
            }
        }

        if (pending_job.is_null()) {
            res.status = 204; // No Content
            return;
        }

        // Find an idle engine with benchmarking data
        nlohmann::json selected_engine = nullptr;
        std::vector<nlohmann::json> available_engines;
        for (auto const& [eng_id, eng_data] : engines_db.items()) {
            if (eng_data["status"] == "idle" && eng_data.contains("benchmark_time")) {
                available_engines.push_back(eng_data);
            }
        }

        if (available_engines.empty()) {
            res.status = 204; // No Content
            return;
        }
        
        // Sort engines by benchmark_time (faster engines first)
        std::sort(available_engines.begin(), available_engines.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
            return a["benchmark_time"] < b["benchmark_time"];
        });

        double job_size = pending_job.value("job_size", 0.0);
        double large_job_threshold = 100.0;

        if (job_size >= large_job_threshold) {
            nlohmann::json streaming_capable_engine = nullptr;
            for (const auto& eng : available_engines) {
                if (eng.value("streaming_support", false)) {
                    streaming_capable_engine = eng;
                    break;
                }
            }
            if (!streaming_capable_engine.is_null()) {
                selected_engine = streaming_capable_engine;
            } else {
                selected_engine = available_engines[0]; // Fastest available
            }
        } else {
            double small_job_threshold = 50.0;
            if (job_size < small_job_threshold) {
                selected_engine = available_engines.back(); // Slowest
            } else {
                selected_engine = available_engines[0]; // Fastest
            }
        }

        if (selected_engine.is_null()) {
            res.status = 204; // No Content
            return;
        }

        // Assign the job
        jobs_db[pending_job["job_id"].get<std::string>()]["status"] = "assigned";
        jobs_db[pending_job["job_id"].get<std::string>()]["assigned_engine"] = selected_engine["engine_id"];
        engines_db[selected_engine["engine_id"].get<std::string>()]["status"] = "busy";
        
        save_state_unlocked();

        res.set_content(jobs_db[pending_job["job_id"].get<std::string>()].dump(), "application/json");
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
