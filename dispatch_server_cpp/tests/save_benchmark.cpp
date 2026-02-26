#include <iostream>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include "dispatch_server_core.h"
#include "nlohmann/json.hpp"

using namespace distconv::DispatchServer;

void populate_legacy_jobs(int count) {
    std::lock_guard<std::mutex> lock(state_mutex);
    jobs_db.clear();
    for (int i = 0; i < count; ++i) {
        nlohmann::json job;
        job["id"] = "job_" + std::to_string(i);
        job["status"] = "pending";
        job["source_url"] = "http://example.com/video_" + std::to_string(i) + ".mp4";
        job["priority"] = i % 3;
        jobs_db["job_" + std::to_string(i)] = job;
    }
}

void benchmark_legacy_save(int count) {
    std::cout << "Populating legacy jobs (" << count << ")..." << std::endl;
    populate_legacy_jobs(count);

    std::cout << "Running legacy save..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    save_state();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "[Baseline] Legacy save_state() with " << count << " jobs: " << elapsed.count() << " ms" << std::endl;
}

void benchmark_modern_shutdown(int count) {
    std::cout << "Setting up modern server (" << count << " jobs)..." << std::endl;
    std::string db_file = "benchmark_jobs_" + std::to_string(count) + ".db";
    std::string engine_file = "benchmark_engines_" + std::to_string(count) + ".db";
    std::remove(db_file.c_str());
    std::remove(engine_file.c_str());

    auto job_repo = std::make_shared<SqliteJobRepository>(db_file);
    auto engine_repo = std::make_shared<SqliteEngineRepository>(engine_file);

    DispatchServer server(job_repo, engine_repo);

    // Note: We populate repo one by one which is slow due to SQLite setup overhead.
    // Ideally we would batch insert, but the repository interface doesn't support it yet.
    // This setup time is NOT part of the shutdown benchmark.
    for (int i = 0; i < count; ++i) {
        if (i % 100 == 0) std::cout << "." << std::flush;
        nlohmann::json job;
        job["job_id"] = "job_" + std::to_string(i);
        job["status"] = "pending";
        job["source_url"] = "http://example.com/video_" + std::to_string(i) + ".mp4";
        job["priority"] = i % 3;
        job_repo->save_job(job["job_id"], job);
    }
    std::cout << std::endl;

    std::cout << "Starting server..." << std::endl;
    server.start(0, false);

    // Sleep briefly to let background worker start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Stopping server..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    server.stop();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "[Optimized] Modern DispatchServer::stop() with " << count << " jobs: " << elapsed.count() << " ms" << std::endl;

    std::remove(db_file.c_str());
    std::remove(engine_file.c_str());
}

int main() {
    PERSISTENT_STORAGE_FILE = "benchmark_dispatch_server_state.json";

    std::cout << "Starting Performance Benchmark..." << std::endl;

    // Run with 100 items
    benchmark_legacy_save(100);
    benchmark_modern_shutdown(100);

    // Run with 1000 items
    benchmark_legacy_save(1000);
    benchmark_modern_shutdown(1000);

    std::remove(PERSISTENT_STORAGE_FILE.c_str());

    return 0;
}
