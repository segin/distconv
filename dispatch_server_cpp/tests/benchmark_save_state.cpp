#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstdio>
#include "dispatch_server_core.h"
#include "nlohmann/json.hpp"

using namespace distconv::DispatchServer;

void SetupLargeDB() {
    std::lock_guard<std::mutex> lock(state_mutex);
    jobs_db.clear();
    engines_db.clear();

    // Create 10,000 jobs
    for (int i = 0; i < 10000; ++i) {
        nlohmann::json job;
        job["job_id"] = "job_" + std::to_string(i);
        job["source_url"] = "http://example.com/video_" + std::to_string(i) + ".mp4";
        job["status"] = "pending";
        job["created_at"] = 1234567890;
        jobs_db[job["job_id"]] = job;
    }

    // Create 100 engines
    for (int i = 0; i < 100; ++i) {
        nlohmann::json engine;
        engine["engine_id"] = "engine_" + std::to_string(i);
        engine["status"] = "idle";
        engines_db[engine["engine_id"]] = engine;
    }
}

int main() {
    SetupLargeDB();
    PERSISTENT_STORAGE_FILE = "benchmark_state.json";

    start_persistence_thread();
    // Allow thread to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int iterations = 10;
    std::cout << "Running benchmark with " << iterations << " iterations..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        std::lock_guard<std::mutex> lock(state_mutex);
        save_state_unlocked(true); // Explicitly async
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    std::cout << "Total time for " << iterations << " iterations: " << duration.count() << " ms" << std::endl;
    std::cout << "Average time per save: " << duration.count() / iterations << " ms" << std::endl;

    stop_persistence_thread();
    std::remove("benchmark_state.json");
    return 0;
}
