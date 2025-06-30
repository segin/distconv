#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "dispatch_server_core.h" // Include the new header

// Global API Key
std::string API_KEY = "";

// In-memory storage for jobs and engines (for now)
// In a real application, this would be a database
nlohmann::json jobs_db = nlohmann::json::object();
nlohmann::json engines_db = nlohmann::json::object();

// Persistent storage for jobs and engines
const std::string PERSISTENT_STORAGE_FILE = "dispatch_server_state.json";

void load_state() {
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

void save_state() {
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

int run_dispatch_server(int argc, char* argv[]) {
    std::string cert_path = "server.crt"; // These are no longer used for httplib::Server
    std::string key_path = "server.key";   // but kept for argument parsing consistency.

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cert" && i + 1 < argc) {
            cert_path = argv[++i];
        } else if (arg == "--key" && i + 1 < argc) {
            key_path = argv[++i];
        } else if (arg == "--api-key" && i + 1 < argc) {
            API_KEY = argv[++i];
        }
    }

    httplib::Server svr; // Use non-SSL server

    // Load state on startup
    load_state();

    // Endpoint to submit a new transcoding job
    svr.Post("/jobs/", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
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

            jobs_db[job_id] = job;
            save_state();

            res.set_content(job.dump(), "application/json");
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint to get job status
    svr.Get(R"(/jobs/(\w+))", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        std::string job_id = req.matches[1];
        if (jobs_db.contains(job_id)) {
            res.set_content(jobs_db[job_id].dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content("Job not found", "text/plain");
        }
    });

    // Endpoint to list all jobs
    svr.Get("/jobs/", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        nlohmann::json all_jobs = nlohmann::json::array();
        for (auto const& [key, val] : jobs_db.items()) {
            all_jobs.push_back(val);
        }
        res.set_content(all_jobs.dump(), "application/json");
    });

    // Endpoint to list all engines
    svr.Get("/engines/", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        nlohmann::json all_engines = nlohmann::json::array();
        for (auto const& [key, val] : engines_db.items()) {
            all_engines.push_back(val);
        }
        res.set_content(all_engines.dump(), "application/json");
    });

    // Endpoint for engine heartbeat
    svr.Post("/engines/heartbeat", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            std::string engine_id = request_json["engine_id"];
            engines_db[engine_id] = request_json;
            save_state();
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
    svr.Post("/engines/benchmark_result", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            std::string engine_id = request_json["engine_id"];
            if (engines_db.contains(engine_id)) {
                engines_db[engine_id]["benchmark_time"] = request_json["benchmark_time"];
                save_state();
                res.set_content("Benchmark result received from engine " + engine_id, "text/plain");
            } else {
                res.status = 404;
                res.set_content("Engine not found", "text/plain");
            }
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint to complete a job
    svr.Post(R"(/jobs/(\w+)/complete)", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        std::string job_id = req.matches[1];
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (jobs_db.contains(job_id)) {
                jobs_db[job_id]["status"] = "completed";
                jobs_db[job_id]["output_url"] = request_json["output_url"];
                save_state();
                res.set_content("Job " + job_id + " marked as completed", "text/plain");
            } else {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
            }
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint to fail a job
    svr.Post(R"(/jobs/(\w+)/fail)", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        std::string job_id = req.matches[1];
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (jobs_db.contains(job_id)) {
                jobs_db[job_id]["retries"] = jobs_db[job_id].value("retries", 0) + 1;
                if (jobs_db[job_id]["retries"] <= jobs_db[job_id].value("max_retries", 3)) {
                    jobs_db[job_id]["status"] = "pending"; // Re-queue
                    jobs_db[job_id]["error_message"] = request_json["error_message"];
                    res.set_content("Job " + job_id + " re-queued", "text/plain");
                } else {
                    jobs_db[job_id]["status"] = "failed_permanently";
                    jobs_db[job_id]["error_message"] = request_json["error_message"];
                    res.set_content("Job " + job_id + " failed permanently", "text/plain");
                }
                save_state();
            } else {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
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
    svr.Post("/assign_job/", [&](const httplib::Request& req, httplib::Response& res) {
        std::string engine_id = "";
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            engine_id = request_json["engine_id"];
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
            return;
        }

        nlohmann::json pending_job = nullptr;
        for (auto const& [job_key, job_val] : jobs_db.items()) {
            if (job_val["status"] == "pending") {
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
        jobs_db[pending_job["job_id"]]["status"] = "assigned";
        jobs_db[pending_job["job_id"]]["assigned_engine"] = selected_engine["engine_id"];
        engines_db[selected_engine["engine_id"]]["status"] = "busy";
        save_state();

        res.set_content(pending_job.dump(), "application/json");
    });

    // Placeholder for storage pool configuration (to be implemented later)
    svr.Get("/storage_pools/", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content("Storage pool configuration to be implemented.", "text/plain");
    });

    std::cout << "Dispatch Server listening on port 8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    // Save state on shutdown (this part might not be reached cleanly on SIGINT)
    save_state();

    return 0;
}

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include "httplib.h"
#include "nlohmann/json.hpp"

// Global API Key
std::string API_KEY = "";

// In-memory storage for jobs and engines (for now)
// In a real application, this would be a database
nlohmann::json jobs_db = nlohmann::json::object();
nlohmann::json engines_db = nlohmann::json::object();

// Persistent storage for jobs and engines
const std::string PERSISTENT_STORAGE_FILE = "dispatch_server_state.json";

void load_state() {
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

void save_state() {
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

int main(int argc, char* argv[]) {
    std::string cert_path = "server.crt";
    std::string key_path = "server.key";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cert" && i + 1 < argc) {
            cert_path = argv[++i];
        } else if (arg == "--key" && i + 1 < argc) {
            key_path = argv[++i];
        } else if (arg == "--api-key" && i + 1 < argc) {
            API_KEY = argv[++i];
        }
    }

    httplib::SSLServer svr(cert_path, key_path);

    // Load state on startup
    load_state();

    // Endpoint to submit a new transcoding job
    svr.Post("/jobs/", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
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

            jobs_db[job_id] = job;
            save_state();

            res.set_content(job.dump(), "application/json");
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint to get job status
    svr.Get(R"(/jobs/(\w+))", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        std::string job_id = req.matches[1];
        if (jobs_db.contains(job_id)) {
            res.set_content(jobs_db[job_id].dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content("Job not found", "text/plain");
        }
    });

    // Endpoint to list all jobs
    svr.Get("/jobs/", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        nlohmann::json all_jobs = nlohmann::json::array();
        for (auto const& [key, val] : jobs_db.items()) {
            all_jobs.push_back(val);
        }
        res.set_content(all_jobs.dump(), "application/json");
    });

    // Endpoint to list all engines
    svr.Get("/engines/", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        nlohmann::json all_engines = nlohmann::json::array();
        for (auto const& [key, val] : engines_db.items()) {
            all_engines.push_back(val);
        }
        res.set_content(all_engines.dump(), "application/json");
    });

    // Endpoint for engine heartbeat
    svr.Post("/engines/heartbeat", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            std::string engine_id = request_json["engine_id"];
            engines_db[engine_id] = request_json;
            save_state();
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
    svr.Post("/engines/benchmark_result", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            std::string engine_id = request_json["engine_id"];
            if (engines_db.contains(engine_id)) {
                engines_db[engine_id]["benchmark_time"] = request_json["benchmark_time"];
                save_state();
                res.set_content("Benchmark result received from engine " + engine_id, "text/plain");
            } else {
                res.status = 404;
                res.set_content("Engine not found", "text/plain");
            }
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint to complete a job
    svr.Post(R"(/jobs/(\w+)/complete)", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        std::string job_id = req.matches[1];
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (jobs_db.contains(job_id)) {
                jobs_db[job_id]["status"] = "completed";
                jobs_db[job_id]["output_url"] = request_json["output_url"];
                save_state();
                res.set_content("Job " + job_id + " marked as completed", "text/plain");
            } else {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
            }
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    });

    // Endpoint to fail a job
    svr.Post(R"(/jobs/(\w+)/fail)", [&](const httplib::Request& req, httplib::Response& res) {
        if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }
        std::string job_id = req.matches[1];
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (jobs_db.contains(job_id)) {
                jobs_db[job_id]["retries"] = jobs_db[job_id].value("retries", 0) + 1;
                if (jobs_db[job_id]["retries"] <= jobs_db[job_id].value("max_retries", 3)) {
                    jobs_db[job_id]["status"] = "pending"; // Re-queue
                    jobs_db[job_id]["error_message"] = request_json["error_message"];
                    res.set_content("Job " + job_id + " re-queued", "text/plain");
                } else {
                    jobs_db[job_id]["status"] = "failed_permanently";
                    jobs_db[job_id]["error_message"] = request_json["error_message"];
                    res.set_content("Job " + job_id + " failed permanently", "text/plain");
                }
                save_state();
            } else {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
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
    svr.Post("/assign_job/", [&](const httplib::Request& req, httplib::Response& res) {
        std::string engine_id = "";
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            engine_id = request_json["engine_id"];
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
            return;
        }

        nlohmann::json pending_job = nullptr;
        for (auto const& [job_key, job_val] : jobs_db.items()) {
            if (job_val["status"] == "pending") {
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
        jobs_db[pending_job["job_id"]]["status"] = "assigned";
        jobs_db[pending_job["job_id"]]["assigned_engine"] = selected_engine["engine_id"];
        engines_db[selected_engine["engine_id"]]["status"] = "busy";
        save_state();

        res.set_content(pending_job.dump(), "application/json");
    });

    // Placeholder for storage pool configuration (to be implemented later)
    svr.Get("/storage_pools/", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content("Storage pool configuration to be implemented.", "text/plain");
    });

    std::cout << "Dispatch Server listening on port 8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    // Save state on shutdown (this part might not be reached cleanly on SIGINT)
    save_state();

    return 0;
}
