#include "../repositories.h"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <vector>
#include <random>

using namespace distconv::DispatchServer;

void run_benchmark(int iterations) {
    std::string db_path = "benchmark_test.db";

    // Clean up previous run
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    std::cout << "Initializing Repository..." << std::endl;
    SqliteJobRepository repo(db_path);

    // Prepare data
    nlohmann::json job_template = {
        {"id", "job_1"},
        {"status", "pending"},
        {"priority", 10},
        {"command", "ffmpeg -i input.mp4 output.mp4"},
        {"created_at", 1234567890}
    };

    std::cout << "Starting Write Benchmark (" << iterations << " iterations)..." << std::endl;
    auto start_write = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        std::string id = "job_" + std::to_string(i);
        job_template["id"] = id;
        repo.save_job(id, job_template);
    }

    auto end_write = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> write_diff = end_write - start_write;
    double write_ops = iterations / write_diff.count();

    std::cout << "Write Time: " << write_diff.count() << "s" << std::endl;
    std::cout << "Write Ops/sec: " << write_ops << std::endl;

    std::cout << "Starting Read Benchmark (" << iterations << " iterations)..." << std::endl;
    auto start_read = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        std::string id = "job_" + std::to_string(i);
        auto job = repo.get_job(id);
        if (job.empty()) {
            std::cerr << "Error: Job not found!" << std::endl;
        }
    }

    auto end_read = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> read_diff = end_read - start_read;
    double read_ops = iterations / read_diff.count();

    std::cout << "Read Time: " << read_diff.count() << "s" << std::endl;
    std::cout << "Read Ops/sec: " << read_ops << std::endl;

    // Cleanup
    std::filesystem::remove(db_path);
}

int main(int argc, char** argv) {
    int iterations = 1000;
    if (argc > 1) {
        iterations = std::atoi(argv[1]);
    }

    try {
        run_benchmark(iterations);
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
