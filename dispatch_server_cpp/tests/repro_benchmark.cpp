#include "../repositories.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>

using namespace distconv::DispatchServer;

int main() {
    std::string db_path = "benchmark_test.db";

    // Clean up previous run
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    try {
        std::cout << "Initializing SqliteJobRepository..." << std::endl;
        SqliteJobRepository repo(db_path);

        int iterations = 1000;
        std::cout << "Starting benchmark with " << iterations << " iterations..." << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i) {
            std::string job_id = "job_" + std::to_string(i);
            nlohmann::json job;
            job["job_id"] = job_id;
            job["status"] = "pending";
            job["priority"] = i % 10;
            job["created_at"] = 1000 + i;

            repo.save_job(job_id, job);

            nlohmann::json retrieved_job = repo.get_job(job_id);
            if (retrieved_job.empty()) {
                std::cerr << "Failed to retrieve job " << job_id << std::endl;
                return 1;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        std::cout << "Benchmark completed in " << duration << " ms" << std::endl;
        std::cout << "Average time per operation (save+get): " << (double)duration / iterations << " ms" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    // Clean up
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    return 0;
}
