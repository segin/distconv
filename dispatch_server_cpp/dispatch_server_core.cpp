#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "dispatch_server_core.h" // Include the new header

// In-memory storage for jobs and engines (for now)
// In a real application, this would be a database
nlohmann::json jobs_db = nlohmann::json::object();
nlohmann::json engines_db = nlohmann::json::object();
std::mutex state_mutex; // Single mutex for all shared state

// Persistent storage for jobs and engines
std::string PERSISTENT_STORAGE_FILE = "dispatch_server_state.json";

bool mock_save_state_enabled = false;
int save_state_call_count = 0;

// Forward declaration for save_state_unlocked
void save_state_unlocked();

void load_state() {
    std::lock_guard<std::mutex> lock(state_mutex);
    jobs_db.clear();
    engines_db.clear();
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    if (ifs.is_open()) {
        try {
            nlohmann::json state = nlohmann::json::parse(ifs);
            if (state.contains("jobs")) {
                jobs_db = state["jobs"];
            }
            if (state.contains("engines")) {
                engines_db = state["engines"];
            }
            std::cout << "Loaded state: jobs=" << jobs_db.size() << ", engines=" << engines_db.size() << std::endl;
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "Error parsing persistent state file: " << e.what() << std::endl;
        }
        ifs.close();
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

void setup_endpoints(httplib::Server &svr, const std::string& api_key); // Forward declaration

DispatchServer::DispatchServer() {
    // Endpoints will be set up when set_api_key is called
}

void DispatchServer::set_api_key(const std::string& key) {
    api_key_ = key;
    // Re-setup endpoints with the new API key
    setup_endpoints(svr, api_key_);
}

void DispatchServer::start(int port, bool block) {
    if (block) {
        svr.listen("0.0.0.0", port);
    } else {
        server_thread = std::thread([this, port]() {
            this->svr.listen("0.0.0.0", port);
        });
    }
}

void DispatchServer::stop() {
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

            std::string job_id = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            
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

            {
                std::lock_guard<std::mutex> lock(state_mutex);
                jobs_db[job_id] = job;
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
