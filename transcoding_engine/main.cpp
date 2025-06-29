#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib> // For system()
#include <random> // For random engine ID
#include "cjson/cJSON.h" // Include cJSON header

// For HTTP requests (using system curl for simplicity)
// In a real application, a proper C++ HTTP client library would be used.

// Function to send heartbeat to dispatch server
void sendHeartbeat(const std::string& dispatchServerUrl, const std::string& engineId, double storageCapacityGb) {
    std::string command = "curl -X POST " + dispatchServerUrl + "/engines/heartbeat "
                          "-H \"Content-Type: application/json\" "
                          "-d '{\"engine_id\": \"" + engineId + "\", \"status\": \"idle\", \"storage_capacity_gb\": " + std::to_string(storageCapacityGb) + "}'";
    std::cout << "Sending heartbeat: " << command << std::endl;
    // system(command.c_str()); // Execute the command
}

// Function to download a file
bool downloadFile(const std::string& url, const std::string& output_path) {
    std::string command = "curl -o " + output_path + " " + url;
    std::cout << "Downloading: " << command << std::endl;
    int result = system(command.c_str());
    return result == 0;
}

// Function to upload a file (placeholder)
bool uploadFile(const std::string& url, const std::string& file_path) {
    std::cout << "Uploading " << file_path << " to " << url << " (placeholder)" << std::endl;
    // In a real scenario, this would involve a proper HTTP PUT/POST or a dedicated upload mechanism.
    // For now, just simulate success.
    return true;
}

// Function to report job status to dispatch server
void reportJobStatus(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& status, const std::string& output_url = "", const std::string& error_message = "") {
    std::string command;
    if (status == "completed") {
        command = "curl -X POST " + dispatchServerUrl + "/jobs/" + job_id + "/complete "
                  "-H \"Content-Type: application/json\" "
                  "-d '{\"output_url\": \"" + output_url + "\"}'";
    } else if (status == "failed") {
        command = "curl -X POST " + dispatchServerUrl + "/jobs/" + job_id + "/fail "
                  "-H \"Content-Type: application/json\" "
                  "-d '{\"error_message\": \"" + error_message + "\"}'";
    }
    std::cout << "Reporting job status: " << command << std::endl;
    system(command.c_str());
}

// Actual transcoding logic
void performTranscoding(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& source_url, const std::string& target_codec) {
    std::cout << "Starting transcoding for job " << job_id << ": " << source_url << " to " << target_codec << std::endl;

    std::string input_file = "input_" + job_id + ".mp4";
    std::string output_file = "output_" + job_id + ".mp4";

    // 1. Download the source video
    if (!downloadFile(source_url, input_file)) {
        reportJobStatus(dispatchServerUrl, job_id, "failed", "", "Failed to download source video.");
        return;
    }

    // 2. Execute ffmpeg
    std::string ffmpeg_command = "ffmpeg -i " + input_file + " -c:v " + target_codec + " " + output_file;
    std::cout << "Executing: " << ffmpeg_command << std::endl;
    int ffmpeg_result = system(ffmpeg_command.c_str());

    if (ffmpeg_result != 0) {
        reportJobStatus(dispatchServerUrl, job_id, "failed", "", "FFmpeg transcoding failed.");
        return;
    }

    // 3. Upload the transcoded file (placeholder for now)
    std::string output_url = "http://example.com/transcoded/" + output_file; // Placeholder URL
    if (!uploadFile(output_url, output_file)) {
        reportJobStatus(dispatchServerUrl, job_id, "failed", "", "Failed to upload transcoded video.");
        return;
    }

    // 4. Report job completion
    reportJobStatus(dispatchServerUrl, job_id, "completed", output_url);

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
void sendBenchmarkResult(const std::string& dispatchServerUrl, const std::string& engineId, double benchmark_time) {
    std::string command = "curl -X POST " + dispatchServerUrl + "/engines/benchmark_result "
                          "-H \"Content-Type: application/json\" "
                          "-d '{\"engine_id\": \"" + engineId + "\", \"benchmark_time\": " + std::to_string(benchmark_time) + "}'";
    std::cout << "Sending benchmark result: " << command << std::endl;
    // system(command.c_str()); // Execute the command
}

// Function to get a job from the dispatch server
std::string getJob(const std::string& dispatchServerUrl, const std::string& engineId) {
    std::string command = "curl -X POST " + dispatchServerUrl + "/assign_job/ "
                          "-H \"Content-Type: application/json\" "
                          "-d '{\"engine_id\": \"" + engineId + "\"}'";
    
    // Execute the curl command and capture its output
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }
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

int main() {
    std::cout << "Transcoding Engine Starting..." << std::endl;

    // Configuration
    std::string dispatchServerUrl = "http://localhost:8000"; // Assuming dispatch server runs on localhost:8000
    std::string engineId = "engine-" + std::to_string(std::random_device{}() % 10000); // Unique ID for this engine
    double storageCapacityGb = 500.0; // Example storage capacity

    // Start heartbeat thread
    std::thread heartbeatThread([&]() {
        while (true) {
            sendHeartbeat(dispatchServerUrl, engineId, storageCapacityGb);
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Send heartbeat every 5 seconds
        }
    });
    heartbeatThread.detach(); // Detach to run in background

    // Start benchmarking thread
    std::thread benchmarkThread([&]() {
        while (true) {
            double benchmark_time = performBenchmark();
            sendBenchmarkResult(dispatchServerUrl, engineId, benchmark_time);
            std::this_thread::sleep_for(std::chrono::minutes(5)); // Run benchmark every 5 minutes
        }
    });
    benchmarkThread.detach(); // Detach to run in background

    // Main loop for listening for jobs
    std::cout << "Engine " << engineId << " is idle, waiting for jobs..." << std::endl;
    while (true) {
        std::string job_json = getJob(dispatchServerUrl, engineId);
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

                    performTranscoding(dispatchServerUrl, job_id, source_url, target_codec);
                } else {
                    std::cout << "Failed to parse job details from JSON: " << job_json << std::endl;
                }
                cJSON_Delete(root);
            } else {
                std::cout << "Failed to parse JSON response from getJob: " << job_json << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Poll for jobs every second
    }

    return 0;
}