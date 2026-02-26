#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdio>
#include "repositories.h"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;
using namespace distconv::DispatchServer;

void setup_benchmark_db(SqliteJobRepository& repo, int completed_count, int pending_count) {
    repo.clear_all_jobs();

    // Insert completed jobs
    for (int i = 0; i < completed_count; ++i) {
        nlohmann::json job;
        job["job_id"] = "completed_" + std::to_string(i);
        job["status"] = "completed";
        job["created_at"] = 1000 + i;
        job["priority"] = 0;
        repo.save_job(job["job_id"], job);
    }

    // Insert pending jobs (interspersed or at the end, let's put them at the end for worst case linear scan if it scans in order)
    // Actually get_all_jobs orders by created_at DESC. So latest created are first.
    // If we want to simulate a realistic scenario, pending jobs might be new.
    for (int i = 0; i < pending_count; ++i) {
        nlohmann::json job;
        job["job_id"] = "pending_" + std::to_string(i);
        job["status"] = "pending";
        job["created_at"] = 20000 + i; // Newer than completed
        job["priority"] = 0;
        repo.save_job(job["job_id"], job);
    }
}

void benchmark_linear_scan(SqliteJobRepository& repo) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<nlohmann::json> jobs = repo.get_all_jobs();
    nlohmann::json selected_job;

    for (const auto& job : jobs) {
        if (job["status"] == "pending") {
            selected_job = job;
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    if (selected_job.is_null()) {
        std::cout << "Linear Scan: Found nothing (Unexpected!) in " << elapsed.count() << " ms\n";
    } else {
        std::cout << "Linear Scan: Found job " << selected_job["job_id"] << " in " << elapsed.count() << " ms\n";
    }
}

void benchmark_optimized_query(SqliteJobRepository& repo) {
    auto start = std::chrono::high_resolution_clock::now();

    nlohmann::json selected_job = repo.get_next_pending_job({});

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    if (selected_job.is_null()) {
        std::cout << "Optimized Query: Found nothing (Unexpected!) in " << elapsed.count() << " ms\n";
    } else {
        std::cout << "Optimized Query: Found job " << selected_job["job_id"] << " in " << elapsed.count() << " ms\n";
    }
}

int main() {
    std::string db_path = "benchmark_jobs.db";
    // Clean up previous run
    if (fs::exists(db_path)) {
        fs::remove(db_path);
    }

    SqliteJobRepository repo(db_path);

    int completed_jobs = 10000;
    int pending_jobs = 10;

    std::cout << "Setting up database with " << completed_jobs << " completed jobs and " << pending_jobs << " pending jobs...\n";
    setup_benchmark_db(repo, completed_jobs, pending_jobs);

    std::cout << "Running benchmarks...\n";

    // Warm up? Maybe not needed for sqlite file io but OS cache might matter.
    // We'll run multiple times.

    std::cout << "--- Run 1 ---\n";
    benchmark_linear_scan(repo);
    benchmark_optimized_query(repo);

    std::cout << "--- Run 2 ---\n";
    benchmark_linear_scan(repo);
    benchmark_optimized_query(repo);

    std::cout << "--- Run 3 ---\n";
    benchmark_linear_scan(repo);
    benchmark_optimized_query(repo);

    // Cleanup
    if (fs::exists(db_path)) {
        fs::remove(db_path);
    }

    return 0;
}
