#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib> // For system()
#include <random> // For random engine ID
#include <fstream> // For file operations (e.g., reading thermal sensor data)
#include <algorithm> // For std::remove
#include "cjson/cJSON.h" // Include cJSON header
#include <curl/curl.h> // Include libcurl header
#include <sqlite3.h> // Include SQLite3 header
#include "transcoding_engine_core.h"

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


// Callback function for libcurl to write received data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Generic function to make HTTP requests using libcurl
std::string makeHttpRequest(const std::string& url, const std::string& method, const std::string& payload, const std::string& caCertPath, const std::string& apiKey) {
    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Set CA certificate for HTTPS
        if (!caCertPath.empty()) {
            curl_easy_setopt(curl, CURLOPT_CAINFO, caCertPath.c_str());
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        } else {
            // Allow non-secure connections if no CA path is provided
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }

        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.length());
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            if (!apiKey.empty()) {
                std::string api_key_header = "X-API-Key: " + apiKey;
                headers = curl_slist_append(headers, api_key_header.c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    return readBuffer;
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

// Function to send heartbeat to dispatch server
// Function to send heartbeat to dispatch server
#include <unistd.h> // For gethostname

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
    std::string url = dispatchServerUrl + "/engines/heartbeat";
    std::string payload = "{\"engine_id\": \"" + engineId + "\", \"status\": \"idle\", \"storage_capacity_gb\": " + std::to_string(storageCapacityGb) + ", \"streaming_support\": " + (streamingSupport ? "true" : "false") + ", \"encoders\": \"" + encoders + "\", \"decoders\": \"" + decoders + "\", \"hwaccels\": \"" + hwaccels + "\", \"cpu_temperature\": " + std::to_string(cpuTemperature) + ", \"local_job_queue\": \"" + localJobQueue + "\", \"hostname\": \"" + hostname + "\"}";
    std::cout << "Sending heartbeat to: " << url << " with payload: " << payload << std::endl;
    makeHttpRequest(url, "POST", payload, caCertPath, apiKey);
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

// Function to report job status to dispatch server
void reportJobStatus(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& status, const std::string& output_url, const std::string& error_message, const std::string& caCertPath, const std::string& apiKey) {

// Placeholder for streaming transcoding logic
void performStreamingTranscoding(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& source_url, const std::string& target_codec, const std::string& caCertPath, const std::string& apiKey);

// Actual transcoding logic
void performTranscoding(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& source_url, const std::string& target_codec, const std::string& caCertPath, const std::string& apiKey);

// Function to download a file
bool downloadFile(const std::string& url, const std::string& output_path, const std::string& caCertPath, const std::string& apiKey) {
    CURL *curl;
    CURLcode res;
    FILE *fp;

    curl = curl_easy_init();
    if (curl) {
        fp = fopen(output_path.c_str(), "wb");
        if (fp) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            if (!caCertPath.empty()) {
                curl_easy_setopt(curl, CURLOPT_CAINFO, caCertPath.c_str());
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
            } else {
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            }

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            }
            fclose(fp);
        }
        curl_easy_cleanup(curl);
    }
    return res == CURLE_OK;
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
    std::string payload;

    if (status == "completed") {
        url = dispatchServerUrl + "/jobs/" + job_id + "/complete";
        payload = "{\"output_url\": \"" + output_url + "\"}";
    } else if (status == "failed") {
        url = dispatchServerUrl + "/jobs/" + job_id + "/fail";
        payload = "{\"error_message\": \"" + error_message + "\"}";
    }
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
    std::string payload = "{\"engine_id\": \"" + engineId + "\", \"benchmark_time\": " + std::to_string(benchmark_time) + "}";
    std::cout << "Sending benchmark result to: " << url << " with payload: " << payload << std::endl;
    makeHttpRequest(url, "POST", payload, caCertPath, apiKey);
}

// Function to get a job from the dispatch server
std::string getJob(const std::string& dispatchServerUrl, const std::string& engineId, const std::string& caCertPath, const std::string& apiKey) {
    std::string url = dispatchServerUrl + "/assign_job/";
    std::string payload = "{\"engine_id\": \"" + engineId + "\"}";
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
    std::vector<std::string> localJobQueue = get_jobs_from_db();

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
            for (const std::string& job_id : localJobQueue) {
                cJSON_AddItemToArray(queue_array, cJSON_CreateString(job_id.c_str()));
            }
            char *queue_str = cJSON_PrintUnformatted(queue_array);
            std::string localJobQueueJson = queue_str;
            cJSON_Delete(queue_array);
            free(queue_str);

            sendHeartbeat(dispatchServerUrl, engineId, storageCapacityGb, streamingSupport, encoders, decoders, hwaccels, cpuTemperature, localJobQueueJson, caCertPath, apiKey, hostname);
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

    // Main loop for listening for jobs
    std::cout << "Engine " << engineId << " is idle, waiting for jobs..." << std::endl;
    while (true) {
        std::string job_json = getJob(dispatchServerUrl, engineId, caCertPath);
        if (!job_json.empty()) {
            cJSON *root = cJSON_Parse(job_json.c_str());
            if (root) {
                cJSON *job_id_json = cJSON_GetObjectItemCaseSensitive(root, "job_id");
                cJSON *source_url_json = cJSON_GetObjectItemCaseSensitive(root, "source_url");
                cJSON *target_codec_json = cJSON_GetObjectItemCaseSensitive(root, "target_codec");

                if (cJSON_IsString(job_id_json) && (job_id_json->valuestring != NULL) &&
                    cJSON_IsString(source_url_json) && (source_url_json->valuestring != NULL) &&
                    cJSON_IsString(target_codec_json) && (target_codec_json->valuestring != NULL)) {

                    std::string job_id = job_id_json->valuestring;
                    std::string source_url = source_url_json->valuestring;
                    std::string target_codec = target_codec_json->valuestring;

                    // Add job to local queue
                    add_job_to_db(job_id);
                    localJobQueue.push_back(job_id);

                    performTranscoding(dispatchServerUrl, job_id, source_url, target_codec, caCertPath);

                    // Remove job from local queue after completion (or failure)
                    remove_job_from_db(job_id);
                    localJobQueue.erase(std::remove(localJobQueue.begin(), localJobQueue.end(), job_id), localJobQueue.end());

                } else {
                    std::cout << "Failed to parse job details from JSON: " << job_json << std::endl;
                }
                cJSON_Delete(root);
            }
            else {
                std::cout << "Failed to parse JSON response from getJob: " << job_json << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Poll for jobs every second
    }

    sqlite3_close(db); // Close SQLite database on shutdown
    return 0;
}
