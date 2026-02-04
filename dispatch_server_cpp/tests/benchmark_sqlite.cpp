#include "../repositories.h"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <vector>

using namespace distconv::DispatchServer;

void run_benchmark() {
    std::string db_path = "benchmark.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    try {
        std::cout << "Running SQLite Job Repository Benchmark..." << std::endl;
        SqliteJobRepository repo(db_path);

        int num_operations = 1000;

        // Benchmark Save
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_operations; ++i) {
            nlohmann::json job;
            job["job_id"] = "job_" + std::to_string(i);
            job["status"] = "pending";
            job["priority"] = 1;
            repo.save_job("job_" + std::to_string(i), job);
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        std::cout << "Save " << num_operations << " jobs: " << duration.count() << " ms ("
                  << (duration.count() / num_operations) << " ms/op)" << std::endl;

        // Benchmark Get
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_operations; ++i) {
            repo.get_job("job_" + std::to_string(i));
        }
        end = std::chrono::high_resolution_clock::now();
        duration = end - start;
        std::cout << "Get " << num_operations << " jobs: " << duration.count() << " ms ("
                  << (duration.count() / num_operations) << " ms/op)" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

int main() {
    run_benchmark();
    return 0;
}
