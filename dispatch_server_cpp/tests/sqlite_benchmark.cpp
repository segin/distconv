#include "repositories.h"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <vector>
#include <random>

using namespace distconv::DispatchServer;

void run_benchmark(const std::string& db_path, int num_ops) {
    // Clean up previous run
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    std::cout << "Initializing repository..." << std::endl;
    SqliteJobRepository repo(db_path);

    std::cout << "Starting benchmark with " << num_ops << " operations..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_ops; ++i) {
        nlohmann::json job = {
            {"job_id", "job_" + std::to_string(i)},
            {"status", "pending"},
            {"priority", i % 3},
            {"created_at", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())}
        };
        repo.save_job("job_" + std::to_string(i), job);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Time taken: " << diff.count() << " s" << std::endl;
    std::cout << "Ops/sec: " << num_ops / diff.count() << std::endl;

    // Verify
    auto jobs = repo.get_all_jobs();
    if (jobs.size() != static_cast<size_t>(num_ops)) {
        std::cerr << "Error: Expected " << num_ops << " jobs, found " << jobs.size() << std::endl;
    }
}

int main(int argc, char** argv) {
    int num_ops = 1000;
    if (argc > 1) {
        num_ops = std::stoi(argv[1]);
    }

    run_benchmark("benchmark.db", num_ops);
    return 0;
}
