#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

const std::string DISPATCH_SERVER_URL = "http://127.0.0.1:8000";
const std::string ENGINE_ID = "engine-1";
const int STORAGE_CAPACITY = 1024; // in MB

void register_engine() {
    cpr::Response r = cpr::Post(cpr::Url{DISPATCH_SERVER_URL + "/engine/register"},
                                cpr::Payload{{"engine_id", ENGINE_ID},
                                             {"storage_capacity", std::to_string(STORAGE_CAPACITY)}});
    std::cout << "Registering engine: " << r.text << std::endl;
}

void send_heartbeat(const std::string& status) {
    cpr::Response r = cpr::Post(cpr::Url{DISPATCH_SERVER_URL + "/engine/heartbeat"},
                                cpr::Payload{{"engine_id", ENGINE_ID},
                                             {"status", status}});
    std::cout << "Sending heartbeat: " << r.text << std::endl;
}

void process_job(const json& job) {
    std::string job_id = job["id"];
    std::string source_file_path = job["source_file_path"];
    std::string output_file_path = "transcoded_" + job_id + ".mp4";

    std::cout << "Processing job " << job_id << std::endl;
    std::cout << "Source file: " << source_file_path << std::endl;

    // Simulate download
    std::cout << "Downloading file..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Simulate transcoding
    std::cout << "Transcoding file..." << std::endl;
    std::string ffmpeg_command = "ffmpeg -i " + source_file_path + " " + output_file_path;
    std::cout << "Running command: " << ffmpeg_command << std::endl;
    // In a real implementation, we would run the command here
    // For now, we just simulate the time it takes
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "Transcoding complete." << std::endl;

    // Notify server
    cpr::Response r = cpr::Post(cpr::Url{DISPATCH_SERVER_URL + "/job/complete"},
                                cpr::Payload{{"job_id", job_id},
                                             {"output_file_path", output_file_path}});
    std::cout << "Notifying server: " << r.text << std::endl;
}

void poll_for_jobs() {
    while (true) {
        send_heartbeat("idle");
        cpr::Response r = cpr::Get(cpr::Url{DISPATCH_SERVER_URL + "/job/next"});
        if (r.status_code == 200 && !r.text.empty()) {
            try {
                json job = json::parse(r.text);
                send_heartbeat("transcoding");
                process_job(job);
            } catch (const json::parse_error& e) {
                std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

int main() {
    register_engine();
    poll_for_jobs();
    return 0;
}
