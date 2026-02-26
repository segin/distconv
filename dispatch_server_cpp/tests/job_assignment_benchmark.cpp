#include <benchmark/benchmark.h>
#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include "repositories.h"
#include "dispatch_server_core.h"
#include "nlohmann/json.hpp"

using namespace distconv::DispatchServer;

// Helper to populate repo
void PopulateRepo(std::shared_ptr<IJobRepository> repo, int num_jobs, int num_pending) {
    repo->clear_all_jobs();

    // Create random jobs
    for (int i = 0; i < num_jobs; ++i) {
        nlohmann::json job;
        std::string job_id = "job_" + std::to_string(i);
        job["job_id"] = job_id;

        // Make some jobs pending, others completed/failed/assigned
        if (i < num_pending) {
            job["status"] = "pending";
        } else {
            job["status"] = "completed";
        }

        job["priority"] = rand() % 10;
        job["created_at"] = 1000 + i; // simplistic timestamp

        repo->save_job(job_id, job);
    }
}

static void BM_LinearScanAssignment(benchmark::State& state) {
    // Setup
    auto repo = std::make_shared<InMemoryJobRepository>();
    int num_jobs = state.range(0);
    int num_pending = state.range(1);
    PopulateRepo(repo, num_jobs, num_pending);

    for (auto _ : state) {
        // Simulation of the code in dispatch_server_core.cpp
        std::vector<nlohmann::json> jobs = repo->get_all_jobs();
        nlohmann::json selected_job;

        for (const auto& job : jobs) {
            if (job["status"] == "pending") {
                selected_job = job;
                break;
            }
        }

        benchmark::DoNotOptimize(selected_job);
    }
}

static void BM_OptimizedAssignment(benchmark::State& state) {
    // Setup
    auto repo = std::make_shared<InMemoryJobRepository>();
    int num_jobs = state.range(0);
    int num_pending = state.range(1);
    PopulateRepo(repo, num_jobs, num_pending);

    for (auto _ : state) {
        nlohmann::json selected_job = repo->get_next_pending_job({});
        benchmark::DoNotOptimize(selected_job);
    }
}

// Register benchmarks
// Arguments: Total Jobs, Pending Jobs
BENCHMARK(BM_LinearScanAssignment)->Args({100, 10})->Args({1000, 100})->Args({10000, 100})->Args({10000, 1});
BENCHMARK(BM_OptimizedAssignment)->Args({100, 10})->Args({1000, 100})->Args({10000, 100})->Args({10000, 1});

BENCHMARK_MAIN();
