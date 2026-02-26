#include "repositories.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>
#include <random>

using namespace distconv::DispatchServer;
namespace fs = std::filesystem;

void run_benchmark(int job_count) {
    std::string db_path = "benchmark_test.db";
    if (fs::exists(db_path)) {
        fs::remove(db_path);
    }

    std::cout << "Initializing database at " << db_path << "..." << std::endl;
    auto repo = std::make_shared<SqliteJobRepository>(db_path);

    std::cout << "Seeding " << job_count << " jobs..." << std::endl;

    // Batch insert would be faster but SqliteJobRepository doesn't support it yet.
    // We'll just loop.
    auto start_seed = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < job_count; ++i) {
        nlohmann::json job;
        job["job_id"] = "job_" + std::to_string(i);
        job["status"] = "pending";
        job["created_at"] = 1234567890 + i;
        job["priority"] = i % 10;
        job["payload"] = std::string(1024, 'x'); // 1KB payload
        repo->save_job(job["job_id"], job);
    }

    auto end_seed = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> seed_elapsed = end_seed - start_seed;
    std::cout << "Seeding took " << seed_elapsed.count() << " seconds." << std::endl;

    // Benchmark get_all_jobs
    std::cout << "Benchmarking get_all_jobs()..." << std::endl;
    auto start_fetch = std::chrono::high_resolution_clock::now();

    auto jobs = repo->get_all_jobs();

    auto end_fetch = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> fetch_elapsed = end_fetch - start_fetch;

    std::cout << "Fetched " << jobs.size() << " jobs." << std::endl;
    std::cout << "Time taken: " << fetch_elapsed.count() << " seconds." << std::endl;
    std::cout << "Throughput: " << jobs.size() / fetch_elapsed.count() << " jobs/sec" << std::endl;

    // Benchmark get_jobs_paginated
    std::cout << "\nBenchmarking get_jobs_paginated(100, 0)..." << std::endl;
    auto start_page = std::chrono::high_resolution_clock::now();

    auto page = repo->get_jobs_paginated(100, 0);

    auto end_page = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> page_elapsed = end_page - start_page;

    std::cout << "Fetched " << page.size() << " jobs." << std::endl;
    std::cout << "Time taken: " << page_elapsed.count() << " seconds." << std::endl;

    // Benchmark get_jobs_by_status("pending") - Should be all
    std::cout << "\nBenchmarking get_jobs_by_status('pending')..." << std::endl;
    auto start_pending = std::chrono::high_resolution_clock::now();

    auto pending_jobs = repo->get_jobs_by_status("pending");

    auto end_pending = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> pending_elapsed = end_pending - start_pending;

    std::cout << "Fetched " << pending_jobs.size() << " pending jobs." << std::endl;
    std::cout << "Time taken: " << pending_elapsed.count() << " seconds." << std::endl;

    // Clean up
    if (fs::exists(db_path)) {
        fs::remove(db_path);
    }
}

int main(int argc, char* argv[]) {
    int job_count = 10000;
    if (argc > 1) {
        job_count = std::stoi(argv[1]);
    }
    run_benchmark(job_count);
    return 0;
}
