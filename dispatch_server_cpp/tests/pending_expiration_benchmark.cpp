#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <random>
#include <filesystem>
#include <sqlite3.h>
#include "repositories.h"
#include "nlohmann/json.hpp"

using namespace distconv::DispatchServer;

// Helper to generate random jobs
void generate_jobs(IJobRepository& repo, int count, int stale_count) {
    std::cout << "Generating " << count << " jobs..." << std::endl;

    for (int i = 0; i < count; ++i) {
        nlohmann::json job;
        std::string job_id = "job_" + std::to_string(i);
        job["job_id"] = job_id;
        job["status"] = "pending";
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        if (i < stale_count) {
             job["created_at"] = now_ms - (48LL * 3600 * 1000);
        } else {
             job["created_at"] = now_ms;
        }
        repo.save_job(job_id, job);
    }
}

void fix_sqlite_timestamps(SqliteJobRepository& repo, int stale_count) {
    for (int i = 0; i < stale_count; ++i) {
        std::string job_id = "job_" + std::to_string(i);
        std::string sql = "UPDATE jobs SET created_at = datetime('now', '-2 days') WHERE job_id = '" + job_id + "';";
        repo.execute_sql(sql);
    }
}

void legacy_expire_pending_jobs(IJobRepository& repo, int64_t timeout_seconds) {
    auto stale_job_ids = repo.get_stale_pending_jobs(timeout_seconds);
    for (const auto& job_id : stale_job_ids) {
        nlohmann::json job = repo.get_job(job_id);
        job["status"] = "expired";
        job["error_message"] = "Job expired after being pending for too long";
        repo.save_job(job_id, job);
    }
}

int main() {
    int total_jobs = 2000;
    int stale_jobs = 1000;
    int64_t timeout_seconds = 24 * 3600;

    // SQLite
    std::string db_path = "pending_benchmark.db";
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);
    {
        std::cout << "\n--- Benchmarking SqliteJobRepository ---" << std::endl;
        SqliteJobRepository repo(db_path);
        generate_jobs(repo, total_jobs, stale_jobs);
        fix_sqlite_timestamps(repo, stale_jobs);

        auto start = std::chrono::high_resolution_clock::now();
        legacy_expire_pending_jobs(repo, timeout_seconds);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> legacy_duration = end - start;
        std::cout << "Legacy Time: " << legacy_duration.count() << " ms" << std::endl;

        // Reset jobs
        std::cout << "Resetting jobs..." << std::endl;
        for (int i = 0; i < stale_jobs; i++) {
            std::string job_id = "job_" + std::to_string(i);
            nlohmann::json job = repo.get_job(job_id);
            job["status"] = "pending";
            repo.save_job(job_id, job);
        }
        fix_sqlite_timestamps(repo, stale_jobs);

        start = std::chrono::high_resolution_clock::now();
        repo.expire_stale_jobs(timeout_seconds);
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> optimized_duration = end - start;
        std::cout << "Optimized Time: " << optimized_duration.count() << " ms" << std::endl;
        if (optimized_duration.count() > 0)
            std::cout << "Speedup: " << legacy_duration.count() / optimized_duration.count() << "x" << std::endl;

        nlohmann::json job = repo.get_job("job_0");
        if (job["status"] == "expired" && job.contains("error_message")) {
            std::cout << "Verification SUCCESS: job_0 is expired and has error_message." << std::endl;
        } else {
            std::cout << "Verification FAILED: job_0 status=" << job["status"] << std::endl;
        }
    }
    if (std::filesystem::exists(db_path)) std::filesystem::remove(db_path);

    // InMemory
    {
        std::cout << "\n--- Benchmarking InMemoryJobRepository ---" << std::endl;
        InMemoryJobRepository repo;
        generate_jobs(repo, total_jobs, stale_jobs);

        auto start = std::chrono::high_resolution_clock::now();
        legacy_expire_pending_jobs(repo, timeout_seconds);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> legacy_duration = end - start;
        std::cout << "Legacy Time: " << legacy_duration.count() << " ms" << std::endl;

        // Reset
        std::cout << "Resetting jobs..." << std::endl;
        for (int i = 0; i < stale_jobs; i++) {
            std::string job_id = "job_" + std::to_string(i);
            nlohmann::json job = repo.get_job(job_id);
            job["status"] = "pending";
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            job["created_at"] = now_ms - (48LL * 3600 * 1000);
            repo.save_job(job_id, job);
        }

        start = std::chrono::high_resolution_clock::now();
        repo.expire_stale_jobs(timeout_seconds);
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> optimized_duration = end - start;
        std::cout << "Optimized Time: " << optimized_duration.count() << " ms" << std::endl;
        if (optimized_duration.count() > 0)
            std::cout << "Speedup: " << legacy_duration.count() / optimized_duration.count() << "x" << std::endl;

        nlohmann::json job = repo.get_job("job_0");
        if (job["status"] == "expired" && job.contains("error_message")) {
            std::cout << "Verification SUCCESS: job_0 is expired and has error_message." << std::endl;
        } else {
            std::cout << "Verification FAILED: job_0 status=" << job["status"] << std::endl;
        }
    }

    return 0;
}
