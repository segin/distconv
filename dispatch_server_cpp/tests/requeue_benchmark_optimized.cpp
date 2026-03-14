#include "../repositories.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>

using namespace distconv::DispatchServer;

int main() {
    std::string db_path = "requeue_benchmark_optimized.db";

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    try {
        SqliteJobRepository repo(db_path);

        int num_jobs = 100;
        std::cout << "Seeding " << num_jobs << " failed jobs..." << std::endl;

        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        for (int i = 0; i < num_jobs; ++i) {
            std::string job_id = "job_" + std::to_string(i);
            nlohmann::json job;
            job["job_id"] = job_id;
            job["status"] = "failed_retry";
            job["retry_after"] = now - 1000; // Ready to be requeued
            repo.save_job(job_id, job);
        }

        std::cout << "Starting optimized requeue benchmark..." << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();

        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        int requeued_count = repo.requeue_ready_jobs(now_ms);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        std::cout << "Optimized requeue of " << requeued_count << " jobs took " << duration << " ms" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    return 0;
}
