#include <iostream>
#include <chrono>
#include <vector>
#include <filesystem>
#include <cstdio>
#include "repositories.h"
#include "nlohmann/json.hpp"

using namespace distconv::DispatchServer;

// Utility to measure time
template<typename Func>
double measure_ms(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void setup_test_data(IJobRepository& repo, int total_jobs) {
    repo.clear_all_jobs();

    // Create completed jobs
    for (int i = 0; i < total_jobs - 1; ++i) {
        nlohmann::json job;
        job["job_id"] = "job_" + std::to_string(i);
        job["status"] = "completed";
        job["created_at"] = 100000 + i;
        job["priority"] = 0;
        repo.save_job(job["job_id"], job);
    }

    // Create one pending job (the one we want to find)
    // Make it older than some, newer than others, or just add it at the end.
    // In the old code (LIFO), get_all_jobs sorted by created_at DESC.
    // If we add the pending job last (highest timestamp), it appears first in get_all_jobs (best case for old code).
    // If we add it first (lowest timestamp), it appears last in get_all_jobs (worst case for old code).
    // The new code uses efficient query to find it regardless.

    // Let's add the pending job "in the middle" time-wise, or just ensure it exists.
    nlohmann::json pending_job;
    pending_job["job_id"] = "job_pending";
    pending_job["status"] = "pending";
    pending_job["created_at"] = 200000;
    pending_job["priority"] = 10;
    repo.save_job(pending_job["job_id"], pending_job);
}

void run_benchmark(IJobRepository& repo, const std::string& repo_name, int total_jobs) {
    std::cout << "--- Benchmarking " << repo_name << " with " << total_jobs << " jobs ---" << std::endl;

    setup_test_data(repo, total_jobs);

    // 1. Old Method: Get All + Scan
    double old_method_time = measure_ms([&]() {
        std::vector<nlohmann::json> jobs = repo.get_all_jobs();
        nlohmann::json selected_job;
        for (const auto& job : jobs) {
            if (job["status"] == "pending") {
                selected_job = job;
                break;
            }
        }
        if (selected_job.is_null()) std::cerr << "Failed to find job (Old)!" << std::endl;
    });

    // 2. New Method: get_next_pending_job
    double new_method_time = measure_ms([&]() {
        std::vector<std::string> engines = {"any_engine"};
        nlohmann::json selected_job = repo.get_next_pending_job(engines);
        if (selected_job.is_null()) std::cerr << "Failed to find job (New)!" << std::endl;
    });

    std::cout << "Old Method (Linear Scan): " << old_method_time << " ms" << std::endl;
    std::cout << "New Method (Optimized):   " << new_method_time << " ms" << std::endl;
    std::cout << "Speedup: " << (old_method_time / new_method_time) << "x" << std::endl;
    std::cout << std::endl;
}

int main() {
    // 1. Test In-Memory Repository
    {
        InMemoryJobRepository memory_repo;
        run_benchmark(memory_repo, "InMemoryJobRepository", 1000);
        run_benchmark(memory_repo, "InMemoryJobRepository", 10000);
    }

    // 2. Test SQLite Repository
    {
        std::string db_file = "benchmark_test.db";
        // Clean up previous run
        std::remove(db_file.c_str());

        {
            SqliteJobRepository sqlite_repo(db_file);
            // Smaller numbers for SQLite as insertion takes time
            run_benchmark(sqlite_repo, "SqliteJobRepository", 1000);
            run_benchmark(sqlite_repo, "SqliteJobRepository", 5000);
        }

        std::remove(db_file.c_str());
    }

    return 0;
}
