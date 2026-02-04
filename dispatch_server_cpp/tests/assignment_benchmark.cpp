#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>
#include <random>
#include "repositories.h"
#include "nlohmann/json.hpp"

using namespace distconv::DispatchServer;

// Helper to generate a random string
std::string random_string(size_t length) {
    auto randchar = []() -> char {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[rand() % max_index];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

void setup_benchmark_data(IJobRepository& repo, int total_jobs, int pending_position) {
    std::cout << "Seeding repository with " << total_jobs << " jobs..." << std::endl;

    // Batch operations if possible, but our interface is one-by-one.
    // We'll just do it in a loop.

    for (int i = 0; i < total_jobs; ++i) {
        nlohmann::json job;
        job["job_id"] = "job_" + std::to_string(i);
        job["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - (total_jobs - i) * 1000; // spread out timestamps
        job["priority"] = 0;

        if (i == pending_position) {
            job["status"] = "pending";
        } else {
            job["status"] = "completed";
        }

        // Add some payload to make deserialization non-trivial
        job["source_url"] = "http://example.com/" + random_string(20);
        job["output_url"] = "http://example.com/out/" + random_string(20);
        job["logs"] = random_string(100);

        repo.save_job(job["job_id"], job);
    }
    std::cout << "Seeding complete." << std::endl;
}

void benchmark_linear_scan(IJobRepository& repo) {
    auto start = std::chrono::high_resolution_clock::now();

    // Logic from dispatch_server_core.cpp
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

    std::cout << "Linear Scan Time: " << elapsed.count() << " ms" << std::endl;
    if (!selected_job.is_null()) {
        std::cout << "Found job: " << selected_job["job_id"] << std::endl;
    } else {
        std::cout << "No pending job found." << std::endl;
    }
}

void benchmark_optimized_query(IJobRepository& repo) {
    auto start = std::chrono::high_resolution_clock::now();

    // Optimized logic
    nlohmann::json selected_job = repo.get_next_pending_job({});

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "Optimized Query Time: " << elapsed.count() << " ms" << std::endl;
    if (!selected_job.is_null()) {
        std::cout << "Found job: " << selected_job["job_id"] << std::endl;
    } else {
        std::cout << "No pending job found." << std::endl;
    }
}

int main(int argc, char** argv) {
    std::string db_path = "benchmark_test.db";

    // Clean up previous run
    std::remove(db_path.c_str());

    {
        SqliteJobRepository repo(db_path);

        // Scenario 1: Many jobs, pending job is recent (near the top of get_all_jobs sorted by created_at DESC?)
        // SqliteJobRepository::get_all_jobs sorts by created_at DESC.
        // If we put the pending job at the end (oldest), linear scan has to scan everything.
        // If we put it at the start (newest), linear scan is fast (best case).
        // Let's test the worst case for linear scan: Pending job is old, or buried deep.

        int total_jobs = 5000;
        int pending_index = 0; // The oldest job (created first)

        setup_benchmark_data(repo, total_jobs, pending_index);

        std::cout << "\n--- Benchmarking ---" << std::endl;
        benchmark_linear_scan(repo);
        benchmark_optimized_query(repo);
    }

    // Cleanup
    std::remove(db_path.c_str());

    return 0;
}
