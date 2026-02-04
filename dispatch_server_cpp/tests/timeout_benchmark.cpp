#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <random>
#include <filesystem>
#include "repositories.h"
#include "nlohmann/json.hpp"

using namespace distconv::DispatchServer;

// Helper to generate random jobs
void generate_jobs(IJobRepository& repo, int count, int timed_out_count) {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // Create random engine
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> status_dist(0, 2); // 0: completed, 1: pending, 2: assigned (active)

    std::cout << "Generating " << count << " jobs (" << timed_out_count << " timed out)..." << std::endl;

    // Batch insert using transaction would be faster, but repo interface processes one by one.
    // We'll just loop.
    for (int i = 0; i < count; ++i) {
        nlohmann::json job;
        std::string job_id = "job_" + std::to_string(i);
        job["job_id"] = job_id;

        if (i < timed_out_count) {
            // Timed out jobs
            job["status"] = "assigned";
            // Set updated_at to 60 minutes ago (timeout is 30 mins)
            job["updated_at"] = now_ms - (60 * 60 * 1000);
            job["assigned_engine"] = "engine_1";
        } else {
            // Other jobs
            int s = status_dist(gen);
            if (s == 0) job["status"] = "completed";
            else if (s == 1) job["status"] = "pending";
            else {
                job["status"] = "assigned";
                // Active, updated recently
                job["updated_at"] = now_ms - (5 * 60 * 1000);
                job["assigned_engine"] = "engine_1";
            }
        }
        repo.save_job(job_id, job);
    }
    std::cout << "Generation complete." << std::endl;
}

int main() {
    std::string db_path = "benchmark_test.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    {
        SqliteJobRepository repo(db_path);

        int total_jobs = 10000;
        int timed_out_jobs = 100;

        generate_jobs(repo, total_jobs, timed_out_jobs);

        // Benchmark Legacy Approach
        std::cout << "Benchmarking Legacy Approach..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();

        auto jobs = repo.get_all_jobs();
        auto now = std::chrono::system_clock::now();
        int found_timed_out = 0;

        for (auto& job : jobs) {
            if (job["status"] == "assigned" || job["status"] == "processing") {
                if (job.contains("updated_at")) {
                    auto updated_at = std::chrono::system_clock::time_point{
                        std::chrono::milliseconds{job["updated_at"]}
                    };
                    auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - updated_at);

                    // JOB_TIMEOUT is 30 minutes
                    if (duration.count() > 30) {
                        found_timed_out++;
                    }
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> legacy_duration = end - start;

        std::cout << "Legacy Approach found " << found_timed_out << " timed out jobs." << std::endl;
        std::cout << "Legacy Time: " << legacy_duration.count() << " ms" << std::endl;

        // Verify correct count
        if (found_timed_out != timed_out_jobs) {
            std::cerr << "Error: Expected " << timed_out_jobs << " timed out jobs, found " << found_timed_out << std::endl;
        }

        // Benchmark Optimized Approach
        std::cout << "Benchmarking Optimized Approach..." << std::endl;
        start = std::chrono::high_resolution_clock::now();

        // Timeout logic: older than now - 30 mins
        int64_t timeout_ms = 30 * 60 * 1000;
        int64_t older_than = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() - timeout_ms;

        auto timed_out_jobs_list = repo.get_timed_out_jobs(older_than);

        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> optimized_duration = end - start;

        std::cout << "Optimized Approach found " << timed_out_jobs_list.size() << " timed out jobs." << std::endl;
        std::cout << "Optimized Time: " << optimized_duration.count() << " ms" << std::endl;

        if (timed_out_jobs_list.size() != timed_out_jobs) {
             std::cerr << "Error: Optimized approach found " << timed_out_jobs_list.size() << " jobs, expected " << timed_out_jobs << std::endl;
        }

        std::cout << "Speedup: " << legacy_duration.count() / optimized_duration.count() << "x" << std::endl;
    }

    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    return 0;
}
