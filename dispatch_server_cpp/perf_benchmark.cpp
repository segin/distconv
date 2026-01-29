#include <iostream>
#include <chrono>
#include <vector>
#include <filesystem>
#include "repositories.h"

using namespace distconv::DispatchServer;

int main() {
    std::string db_path = "benchmark_test.db";

    // Clean up previous run
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    try {
        std::cout << "Initializing Repository..." << std::endl;
        SqliteJobRepository repo(db_path);

        // Seed some data
        std::string job_id = "bench_job_1";
        nlohmann::json job_data = {
            {"job_id", job_id},
            {"status", "pending"},
            {"source_url", "http://example.com/video.mp4"}
        };
        repo.save_job(job_id, job_data);

        const int ITERATIONS = 1000;
        std::cout << "Running benchmark for " << ITERATIONS << " iterations of job_exists()..." << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < ITERATIONS; ++i) {
            bool exists = repo.job_exists(job_id);
            if (!exists) {
                std::cerr << "Error: Job should exist!" << std::endl;
                return 1;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "Total time: " << duration << " ms" << std::endl;
        std::cout << "Average time per call: " << (double)duration / ITERATIONS << " ms" << std::endl;

        // Clean up
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }

    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
