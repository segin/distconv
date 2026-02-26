#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <random>
#include <filesystem>
#include "../repositories.h"
#include "nlohmann/json.hpp"

using namespace distconv::DispatchServer;

void run_benchmark(int num_jobs, int num_queries) {
    std::string db_path = "benchmark_jobs.db";

    // Clean up previous run
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    std::cout << "Initializing database..." << std::endl;
    SqliteJobRepository repo(db_path);

    std::cout << "Seeding " << num_jobs << " jobs..." << std::endl;

    // Seed data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> priority_dist(0, 2);
    std::uniform_real_distribution<> status_dist(0.0, 1.0);

    auto start_seed = std::chrono::high_resolution_clock::now();

    // Use a transaction for faster inserts (if exposed, otherwise we rely on implementation)
    // Since save_job opens/closes DB each time, this might be slow for seeding.
    // However, we want to test the read performance mostly.

    for (int i = 0; i < num_jobs; ++i) {
        nlohmann::json job;
        job["job_id"] = "job_" + std::to_string(i);

        double r = status_dist(gen);
        if (r < 0.2) job["status"] = "pending";
        else if (r < 0.5) job["status"] = "assigned";
        else if (r < 0.8) job["status"] = "completed";
        else job["status"] = "failed";

        job["priority"] = priority_dist(gen);
        job["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - (num_jobs - i) * 1000; // Spread out timestamps

        repo.save_job(job["job_id"], job);

        if (i % 1000 == 0 && i > 0) {
            std::cout << "Seeded " << i << " jobs..." << std::endl;
        }
    }

    auto end_seed = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> seed_elapsed = end_seed - start_seed;
    std::cout << "Seeding complete in " << seed_elapsed.count() << " seconds." << std::endl;

    std::cout << "Running " << num_queries << " queries..." << std::endl;

    auto start_query = std::chrono::high_resolution_clock::now();

    std::vector<std::string> capable_engines; // Empty for now as implementation ignores it currently or we don't need filtering

    for (int i = 0; i < num_queries; ++i) {
        auto job = repo.get_next_pending_job(capable_engines);
        // We act like we didn't pick it up (didn't change status) so we keep querying the same DB state
        // or we could update it.
        // For pure read benchmark, not updating is fine, it tests the finding logic.
    }

    auto end_query = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> query_elapsed = end_query - start_query;

    std::cout << "Benchmark Results:" << std::endl;
    std::cout << "Total Query Time: " << query_elapsed.count() << " seconds" << std::endl;
    std::cout << "Average Query Time: " << (query_elapsed.count() / num_queries) * 1000.0 << " ms" << std::endl;
    std::cout << "Queries Per Second: " << num_queries / query_elapsed.count() << std::endl;

    // Cleanup
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

int main(int argc, char** argv) {
    int num_jobs = 10000;
    int num_queries = 100;

    if (argc > 1) num_jobs = std::stoi(argv[1]);
    if (argc > 2) num_queries = std::stoi(argv[2]);

    try {
        run_benchmark(num_jobs, num_queries);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
