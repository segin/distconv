#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>
#include <random>
#include "repositories.h"
#include "nlohmann/json.hpp"

using namespace distconv::DispatchServer;

void print_result(const std::string& name, long long duration_ms, size_t result_count) {
    std::cout << name << ": " << duration_ms << " ms (Found " << result_count << " jobs)" << std::endl;
}

int main() {
    std::string db_path = "benchmark_test.db";
    // Clean up previous run
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    std::cout << "Initializing Repository..." << std::endl;
    auto repo = std::make_unique<SqliteJobRepository>(db_path);

    // Config
    const int NUM_JOBS = 100000;

    std::cout << "Seeding " << NUM_JOBS << " jobs..." << std::endl;
    auto start_seed = std::chrono::high_resolution_clock::now();

    // We want the pending job to be hard to find for the linear scan (at the end),
    // but easy for the optimized query (which filters by status).
    // Actually, get_all_jobs fetches EVERYTHING regardless of status.
    // So if we have 99,999 completed jobs and 1 pending job,
    // get_all_jobs will deserialize 100,000 JSONs.
    // get_next_pending_job will query WHERE status='pending' LIMIT 1.

    for (int i = 0; i < NUM_JOBS; ++i) {
        nlohmann::json job;
        job["job_id"] = "job_" + std::to_string(i);
        job["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + i; // increasing timestamp
        job["priority"] = 0;

        if (i == NUM_JOBS - 1) {
             job["status"] = "pending";
        } else {
             job["status"] = "completed";
        }

        repo->save_job(job["job_id"], job);

        if (i % 10000 == 0) std::cout << "." << std::flush;
    }
    std::cout << " Done." << std::endl;
    auto end_seed = std::chrono::high_resolution_clock::now();
    std::cout << "Seeding took " << std::chrono::duration_cast<std::chrono::milliseconds>(end_seed - start_seed).count() << " ms" << std::endl;

    // --- Benchmark 1: Linear Scan (Current Implementation) ---
    std::cout << "Running Linear Scan Benchmark..." << std::endl;
    auto start_linear = std::chrono::high_resolution_clock::now();

    std::vector<nlohmann::json> jobs = repo->get_all_jobs();
    nlohmann::json selected_job_linear;

    for (const auto& job : jobs) {
        if (job["status"] == "pending") {
            selected_job_linear = job;
            break;
        }
    }

    auto end_linear = std::chrono::high_resolution_clock::now();
    long long duration_linear = std::chrono::duration_cast<std::chrono::milliseconds>(end_linear - start_linear).count();
    print_result("Linear Scan", duration_linear, selected_job_linear.is_null() ? 0 : 1);

    // --- Benchmark 2: Optimized Query ---
    std::cout << "Running Optimized Query Benchmark..." << std::endl;
    auto start_opt = std::chrono::high_resolution_clock::now();

    nlohmann::json selected_job_opt = repo->get_next_pending_job({});

    auto end_opt = std::chrono::high_resolution_clock::now();
    long long duration_opt = std::chrono::duration_cast<std::chrono::milliseconds>(end_opt - start_opt).count();
    print_result("Optimized Query", duration_opt, selected_job_opt.is_null() ? 0 : 1);

    // Validation
    if (selected_job_linear != selected_job_opt) {
        std::cerr << "ERROR: Results mismatch!" << std::endl;
        if (!selected_job_linear.is_null()) std::cerr << "Linear found: " << selected_job_linear["job_id"] << std::endl;
        if (!selected_job_opt.is_null()) std::cerr << "Optimized found: " << selected_job_opt["job_id"] << std::endl;
    } else {
        std::cout << "Validation Passed: Both methods returned the same result." << std::endl;
    }

    // Clean up
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    return 0;
}
