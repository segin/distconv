#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <chrono>
#include <cstdlib> // For system()
#include <random> // For random engine ID
#include <fstream> // For file operations (e.g., reading thermal sensor data)
#include <algorithm> // For std::remove
#include <mutex>
#include "cjson/cJSON.h" // Include cJSON header
#include <curl/curl.h> // Include libcurl header
#include <sqlite3.h> // Include SQLite3 header
#include "transcoding_engine_core.h"

using json = nlohmann::json;
#include <unistd.h> // For gethostname

namespace distconv {
namespace TranscodingEngine {

sqlite3 *db; // Global SQLite database handle

// SQLite callback function for SELECT queries
static int callback(void *data, int argc, char **argv, char **azColName) {
    std::vector<std::string>* job_ids = static_cast<std::vector<std::string>*>(data);
    for (int i = 0; i < argc; i++) {
        if (std::string(azColName[i]) == "job_id") {
            job_ids->push_back(argv[i] ? argv[i] : "NULL");
        }
    }
    return 0;
}

// Function to initialize SQLite database
void init_sqlite() {
    int rc = sqlite3_open("transcoding_jobs.db", &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    const char *sql = "CREATE TABLE IF NOT EXISTS jobs(job_id TEXT PRIMARY KEY NOT NULL);";
    char *zErrMsg = 0;
    rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    } else {
        std::cout << "SQLite database initialized successfully." << std::endl;
    }
}

// Function to add a job to SQLite
void add_job_to_db(const std::string& job_id) {
    char *sql = sqlite3_mprintf("INSERT OR IGNORE INTO jobs (job_id) VALUES ('%q');", job_id.c_str());
    char *zErrMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error (add_job_to_db): " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    } else {
        std::cout << "Job " << job_id << " added to local DB." << std::endl;
    }
    sqlite3_free(sql);
}

// Function to remove a job from SQLite
void remove_job_from_db(const std::string& job_id) {
    char *sql = sqlite3_mprintf("DELETE FROM jobs WHERE job_id = '%q';", job_id.c_str());
    char *zErrMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error (remove_job_from_db): " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    } else {
        std::cout << "Job " << job_id << " removed from local DB." << std::endl;
    }
    sqlite3_free(sql);
}

// Function to get all jobs from SQLite
std::vector<std::string> get_jobs_from_db() {
    std::vector<std::string> job_ids;
    const char *sql = "SELECT job_id FROM jobs;";
    char *zErrMsg = 0;
    int rc = sqlite3_exec(db, sql, callback, &job_ids, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error (get_jobs_from_db): " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    }
    return job_ids;
}


// Generic function to make HTTP requests using cpr
std::string makeHttpRequest(const std::string& url, const std::string& method, const std::string& payload, const std::string& caCertPath, const std::string& apiKey) {
    cpr::SslOptions sslOpts;
    if (!caCertPath.empty()) {
        sslOpts.SetOption(cpr::ssl::CaInfo{caCertPath});
        sslOpts.SetOption(cpr::ssl::VerifyPeer{true});
        sslOpts.SetOption(cpr::ssl::VerifyHost{true});
    } else {
        sslOpts.SetOption(cpr::ssl::VerifyPeer{false});
        sslOpts.SetOption(cpr::ssl::VerifyHost{false});
    }

    cpr::Header headers{{"Content-Type", "application/json"}};
    if (!apiKey.empty()) {
        headers["X-API-Key"] = apiKey;
    }

    cpr::Response r;
    // Add a reasonable timeout to prevent hanging threads (especially for heartbeats)
    cpr::Timeout timeout{10000};

    if (method == "POST") {
        r = cpr::Post(cpr::Url{url}, cpr::Body{payload}, headers, sslOpts, timeout);
    } else if (method == "GET") {
        r = cpr::Get(cpr::Url{url}, headers, sslOpts, timeout);
    } else {
        std::cerr << "Unsupported method: " << method << std::endl;
        return "";
    }

    if (r.status_code == 0) {
        std::cerr << "Request failed: " << r.error.message << std::endl;
        return "";
    }
    return r.text;
}

// Function to convert job queue to JSON string
std::string generateQueueJson(const std::vector<std::string>& queue) {
    cJSON *queue_array = cJSON_CreateArray();
    for (const std::string& job_id : queue) {
        cJSON_AddItemToArray(queue_array, cJSON_CreateString(job_id.c_str()));
    }
    char *queue_str = cJSON_PrintUnformatted(queue_array);
    std::string queue_json = queue_str;
    cJSON_Delete(queue_array);
    free(queue_str);
    return queue_json;
}

// Function to send heartbeat to dispatch server
// Function to get ffmpeg capabilities
std::string getFFmpegCapabilities(const std::string& capability_type) {
    std::string command = "ffmpeg -hide_banner -" + capability_type + " | grep -E \"^ (DE|EN)C\" | awk '{print $2}' | tr '\n' ',' | sed 's/,$/ /'";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL) {
            result += buffer;
        }
    }
    pclose(pipe);
    return result;
}

// Function to get ffmpeg hardware acceleration methods
std::string getFFmpegHWAccels() {
    std::string command = "ffmpeg -hide_banner -hwaccels | grep -E \"^ (DE|EN)C\" | awk '{print $1}' | tr '\n' ',' | sed 's/,$/ /'";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL) {
            result += buffer;
        }
    }
    pclose(pipe);
    return result;
}

// Function to get the hostname
std::string getHostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    } else {
        return "unknown";
    }
}

// Function to send heartbeat to dispatch server
void sendHeartbeat(const std::string& dispatchServerUrl, const std::string& engineId, double storageCapacityGb, bool streamingSupport, const std::string& encoders, const std::string& decoders, const std::string& hwaccels, double cpuTemperature, const std::string& localJobQueue, const std::string& caCertPath, const std::string& apiKey, const std::string& hostname) {
    // cpr::PostAsync returns cpr::AsyncWrapper<cpr::Response>
    static std::list<cpr::AsyncWrapper<cpr::Response>> pending_heartbeats;
    static std::mutex heartbeats_mutex;

    {
        std::lock_guard<std::mutex> lock(heartbeats_mutex);
        // Clean up finished futures
        pending_heartbeats.remove_if([](cpr::AsyncWrapper<cpr::Response>& f) {
            return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        });
    }

    std::string url = dispatchServerUrl + "/engines/heartbeat";

    json j;
    j["engine_id"] = engineId;
    j["status"] = "idle";
    j["storage_capacity_gb"] = storageCapacityGb;
    j["streaming_support"] = streamingSupport;
    j["encoders"] = encoders;
    j["decoders"] = decoders;
    j["hwaccels"] = hwaccels;
    j["cpu_temperature"] = cpuTemperature;
    j["local_job_queue"] = localJobQueue;
    j["hostname"] = hostname;

    std::string payload = j.dump();
    std::cout << "Sending heartbeat to: " << url << " with payload: " << payload << std::endl;

    // Send heartbeat asynchronously to avoid blocking the main loop
    std::thread([url, payload, caCertPath, apiKey]() {
        makeHttpRequest(url, "POST", payload, caCertPath, apiKey);
    }).detach();
}

// Function to get CPU temperature
double getCpuTemperature() {
#ifdef __linux__
    // Linux: Read from /sys/class/thermal/thermal_zone*/temp
    // This is a simplified approach, a more robust solution would iterate through thermal_zone*.
    std::ifstream thermal_file("/sys/class/thermal/thermal_zone0/temp");
    if (thermal_file.is_open()) {
        std::string line;
        if (std::getline(thermal_file, line)) {
            try {
                return std::stod(line) / 1000.0; // Convert millidegrees Celsius to Celsius
            } catch (const std::exception& e) {
                std::cerr << "Error parsing CPU temperature: " << e.what() << std::endl;
            }
        }
    }
    return -1.0; // Indicate error
#elif __FreeBSD__
    // FreeBSD: Use sysctl
    FILE* pipe = popen("sysctl -n dev.cpu.0.temperature", "r");
    if (pipe) {
        char buffer[128];
        std::string result = "";
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != NULL) {
                result += buffer;
            }
        }
        pclose(pipe);
        try {
            // sysctl returns temperature in Kelvin, convert to Celsius
            return std::stod(result) - 273.15;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing CPU temperature (FreeBSD): " << e.what() << std::endl;
        }
    }
    return -1.0; // Indicate error
#elif _WIN32
    // Windows: More complex, typically involves WMI. Placeholder for now.
    std::cout << "CPU temperature retrieval not implemented for Windows yet." << std::endl;
    return -1.0;
#else
    // Other OS: Not implemented
    return -1.0;
#endif
}

// Function to download a file
bool downloadFile(const std::string& url, const std::string& output_path);

// Function to upload a file (placeholder)
bool uploadFile(const std::string& url, const std::string& file_path);

// Function to download a file
bool downloadFile(const std::string& url, const std::string& output_path, const std::string& caCertPath, const std::string& apiKey) {
    cpr::SslOptions sslOpts;
    if (!caCertPath.empty()) {
        sslOpts.SetOption(cpr::ssl::CaInfo{caCertPath});
        sslOpts.SetOption(cpr::ssl::VerifyPeer{true});
        sslOpts.SetOption(cpr::ssl::VerifyHost{true});
    } else {
        sslOpts.SetOption(cpr::ssl::VerifyPeer{false});
        sslOpts.SetOption(cpr::ssl::VerifyHost{false});
    }

    std::ofstream of(output_path, std::ios::binary);
    if (of.is_open()) {
        cpr::Response r = cpr::Download(of, cpr::Url{url}, sslOpts);
        if (r.status_code == 200) {
            return true;
        } else {
            std::cerr << "Download failed: " << r.status_code << " " << r.error.message << std::endl;
        }
    } else {
        std::cerr << "Failed to open output file: " << output_path << std::endl;
    }
    return false;
}

// Function to upload a file (placeholder)
bool uploadFile(const std::string& url, const std::string& file_path, const std::string& caCertPath, const std::string& apiKey) {
    std::cout << "Uploading " << file_path << " to " << url << " (placeholder)" << std::endl;
    // In a real scenario, this would involve a proper HTTP PUT/POST or a dedicated upload mechanism.
    // For now, just simulate success.
    // makeHttpRequest(url, "POST", file_content, caCertPath); // Example of how it would be used
    return true;
}

// Placeholder for streaming transcoding logic
void performStreamingTranscoding(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& source_url, const std::string& target_codec, const std::string& caCertPath, const std::string& apiKey) {
    std::cout << "Starting streaming transcoding for job " << job_id << ": " << source_url << " to " << target_codec << std::endl;
    // In a real scenario, this would involve:
    // 1. Setting up a streaming input from source_url (e.g., using ffmpeg's network capabilities)
    // 2. Executing ffmpeg to transcode and stream output back to dispatch server or storage
    // 3. Reporting progress/completion/failure via dispatch server API
    std::this_thread::sleep_for(std::chrono::seconds(15)); // Simulate streaming transcoding time
    std::cout << "Finished streaming transcoding for job " << job_id << std::endl;
    reportJobStatus(dispatchServerUrl, job_id, "completed", "http://example.com/streamed_output/" + job_id + ".mp4", "", caCertPath, apiKey);
}

// Function to report job status to dispatch server
void reportJobStatus(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& status, const std::string& output_url, const std::string& error_message, const std::string& caCertPath, const std::string& apiKey) {
    std::string url;
    json j;

    if (status == "completed") {
        url = dispatchServerUrl + "/jobs/" + job_id + "/complete";
        j["output_url"] = output_url;
    } else if (status == "failed") {
        url = dispatchServerUrl + "/jobs/" + job_id + "/fail";
        j["error_message"] = error_message;
    }

    std::string payload = j.dump();
    std::cout << "Reporting job status to: " << url << " with payload: " << payload << std::endl;
    makeHttpRequest(url, "POST", payload, caCertPath, apiKey);
}
void performTranscoding(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& source_url, const std::string& target_codec, const std::string& caCertPath, const std::string& apiKey) {
    std::cout << "Starting transcoding for job " << job_id << ": " << source_url << " to " << target_codec << std::endl;

    std::string input_file = "input_" + job_id + ".mp4";
    std::string output_file = "output_" + job_id + ".mp4";

    // 1. Download the source video
    if (!downloadFile(source_url, input_file, caCertPath, apiKey)) {
        reportJobStatus(dispatchServerUrl, job_id, "failed", "", "Failed to download source video.", caCertPath, apiKey);
        return;
    }

    // 2. Execute ffmpeg
    std::string ffmpeg_command = "ffmpeg -i " + input_file + " -c:v " + target_codec + " " + output_file;
    std::cout << "Executing: " << ffmpeg_command << std::endl;
    int ffmpeg_result = system(ffmpeg_command.c_str());

    if (ffmpeg_result != 0) {
        reportJobStatus(dispatchServerUrl, job_id, "failed", "", "FFmpeg transcoding failed.", caCertPath, apiKey);
        return;
    }

    // 3. Upload the transcoded file (placeholder for now)
    std::string output_url = "http://example.com/transcoded/" + output_file; // Placeholder URL
    if (!uploadFile(output_url, output_file, caCertPath, apiKey)) {
        reportJobStatus(dispatchServerUrl, job_id, "failed", "", "Failed to upload transcoded video.", caCertPath, apiKey);
        return;
    }

    // 4. Report job completion
    reportJobStatus(dispatchServerUrl, job_id, "completed", output_url, "", caCertPath, apiKey);

    std::cout << "Finished transcoding for job " << job_id << std::endl;
}

// Function to perform a benchmark and return the duration
double performBenchmark() {
    std::cout << "Starting benchmark..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    // Simulate a benchmarking task (e.g., transcode a small, known file)
    std::this_thread::sleep_for(std::chrono::seconds(5)); 
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "Benchmark finished in " << duration.count() << " seconds." << std::endl;
    return duration.count();
}

// Function to send benchmark results to dispatch server
void sendBenchmarkResult(const std::string& dispatchServerUrl, const std::string& engineId, double benchmark_time, const std::string& caCertPath, const std::string& apiKey) {
    std::string url = dispatchServerUrl + "/engines/benchmark_result";
    json j;
    j["engine_id"] = engineId;
    j["benchmark_time"] = benchmark_time;
    std::string payload = j.dump();
    std::cout << "Sending benchmark result to: " << url << " with payload: " << payload << std::endl;
    makeHttpRequest(url, "POST", payload, caCertPath, apiKey);
}

// Function to get a job from the dispatch server
std::string getJob(const std::string& dispatchServerUrl, const std::string& engineId, const std::string& caCertPath, const std::string& apiKey) {
    std::string url = dispatchServerUrl + "/assign_job/";
    json j;
    j["engine_id"] = engineId;
    std::string payload = j.dump();
    std::string response = makeHttpRequest(url, "POST", payload, caCertPath, apiKey);
    return response;
}

int run_transcoding_engine(int argc, char* argv[]) {
    std::cout << "Transcoding Engine Starting..." << std::endl;

    // Configuration
    std::string dispatchServerUrl = "http://localhost:8080"; // Assuming C++ dispatch server runs on localhost:8080
    std::string engineId = "engine-" + std::to_string(std::random_device{}() % 10000); // Unique ID for this engine
    double storageCapacityGb = 500.0; // Example storage capacity
    bool streamingSupport = true; // This engine supports streaming
    std::string caCertPath = "server.crt"; // Default Path to CA certificate
    std::string apiKey = ""; // Default API Key
    std::string hostname = getHostname(); // Default hostname

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ca-cert" && i + 1 < argc) {
            caCertPath = argv[++i];
        } else if (arg == "--dispatch-url" && i + 1 < argc) {
            dispatchServerUrl = argv[++i];
        } else if (arg == "--api-key" && i + 1 < argc) {
            apiKey = argv[++i];
        } else if (arg == "--hostname" && i + 1 < argc) {
            hostname = argv[++i];
        }
    }

    // Initialize SQLite database
    init_sqlite();

    // Local job queue (simulated)
    std::mutex queue_mutex;
    std::vector<std::string> localJobQueue = get_jobs_from_db();
    std::mutex localJobQueueMutex;

    // Get ffmpeg capabilities
    std::string encoders = getFFmpegCapabilities("encoders");
    std::string decoders = getFFmpegCapabilities("decoders");
    std::string hwaccels = getFFmpegHWAccels();

    // Start heartbeat thread
    std::thread heartbeatThread([&]() {
        while (true) {
            double cpuTemperature = getCpuTemperature();
            // Convert localJobQueue to JSON string
            cJSON *queue_array = cJSON_CreateArray();
            {
                std::lock_guard<std::mutex> lock(localJobQueueMutex);
                for (const std::string& job_id : localJobQueue) {
                    cJSON_AddItemToArray(queue_array, cJSON_CreateString(job_id.c_str()));
                }
            }
            char *queue_str = cJSON_PrintUnformatted(queue_array);
            localJobQueueJson = queue_str;
            cJSON_Delete(queue_array);
            free(queue_str);

            std::string localJobQueueJson;
            {
                std::lock_guard<std::mutex> lock(jobQueueMutex);
                if (jobQueueDirty) {
                    // Convert localJobQueue to JSON string
                    cJSON *queue_array = cJSON_CreateArray();
                    for (const std::string& job_id : localJobQueue) {
                        cJSON_AddItemToArray(queue_array, cJSON_CreateString(job_id.c_str()));
                    }
                    char *queue_str = cJSON_PrintUnformatted(queue_array);
                    cachedJobQueueJson = queue_str;
                    cJSON_Delete(queue_array);
                    free(queue_str);
                    jobQueueDirty = false;
                }
                localJobQueueJson = cachedJobQueueJson;
            }

            std::string localJobQueueJson;
            {
                std::lock_guard<std::mutex> lock(jobQueueMutex);
                if (jobQueueDirty) {
                    // Convert localJobQueue to JSON string
                    cJSON *queue_array = cJSON_CreateArray();
                    for (const std::string& job_id : localJobQueue) {
                        cJSON_AddItemToArray(queue_array, cJSON_CreateString(job_id.c_str()));
                    }
                    char *queue_str = cJSON_PrintUnformatted(queue_array);
                    cachedJobQueueJson = queue_str;
                    cJSON_Delete(queue_array);
                    free(queue_str);
                    jobQueueDirty = false;
                }
                localJobQueueJson = cachedJobQueueJson;
            }

            {
                std::lock_guard<std::mutex> lock(jobQueueMutex);
                if (jobQueueDirty) {
                    // Convert localJobQueue to JSON string
                    cJSON *queue_array = cJSON_CreateArray();
                    for (const std::string& job_id : localJobQueue) {
                        cJSON_AddItemToArray(queue_array, cJSON_CreateString(job_id.c_str()));
                    }
                    char *queue_str = cJSON_PrintUnformatted(queue_array);
                    cachedJobQueueJson = queue_str ? queue_str : "[]";
                    cJSON_Delete(queue_array);
                    if (queue_str) free(queue_str);
                    jobQueueDirty = false;
                }
                currentJobQueueJson = cachedJobQueueJson;
            }

            sendHeartbeat(dispatchServerUrl, engineId, storageCapacityGb, streamingSupport, encoders, decoders, hwaccels, cpuTemperature, currentJobQueueJson, caCertPath, apiKey, hostname);
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Send heartbeat every 5 seconds
        }
    });
    heartbeatThread.detach(); // Detach to run in background

    // Start benchmarking thread
    std::thread benchmarkThread([&]() {
        while (true) {
            double benchmark_time = performBenchmark();
            sendBenchmarkResult(dispatchServerUrl, engineId, benchmark_time, caCertPath, apiKey);
            std::this_thread::sleep_for(std::chrono::minutes(5)); // Run benchmark every 5 minutes
        }
    });
    benchmarkThread.detach(); // Detach to run in background

    // Initialize backoff timer with 1s min and 30s max delay
    BackoffTimer backoff_timer(std::chrono::seconds(1), std::chrono::seconds(30));

    // Main loop for listening for jobs
    std::cout << "Engine " << engineId << " is idle, waiting for jobs..." << std::endl;
    while (true) {
        std::string job_json = getJob(dispatchServerUrl, engineId, caCertPath, apiKey);
        if (!job_json.empty()) {
            try {
                auto root = json::parse(job_json);
                if (root.contains("job_id") && root.contains("source_url") && root.contains("target_codec") &&
                    root["job_id"].is_string() && root["source_url"].is_string() && root["target_codec"].is_string()) {

                    std::string job_id = root["job_id"];
                    std::string source_url = root["source_url"];
                    std::string target_codec = root["target_codec"];

                    // Reset backoff timer on successful job retrieval
                    backoff_timer.reset();

                    // Add job to local queue
                    add_job_to_db(job_id);
                    {
                        std::lock_guard<std::mutex> lock(localJobQueueMutex);
                        localJobQueue.push_back(job_id);
                    }

                    performTranscoding(dispatchServerUrl, job_id, source_url, target_codec, caCertPath, apiKey);

                    // Remove job from local queue after completion (or failure)
                    remove_job_from_db(job_id);
                    {
                        std::lock_guard<std::mutex> lock(localJobQueueMutex);
                        localJobQueue.erase(std::remove(localJobQueue.begin(), localJobQueue.end(), job_id), localJobQueue.end());
                    }

                } else {
                    std::cout << "Failed to parse job details from JSON: " << job_json << std::endl;
                }
            }
            catch (const json::parse_error& e) {
                std::cout << "Failed to parse JSON response from getJob: " << job_json << " Error: " << e.what() << std::endl;
            }
        }
        // No job found or error occurred, back off
        backoff_timer.sleep();
    }

    sqlite3_close(db); // Close SQLite database on shutdown
    return 0;
}

} // namespace TranscodingEngine
} // namespace distconv
