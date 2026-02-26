#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <future>
#include <random>

using namespace distconv::DispatchServer;

// Global variables for the server
DispatchServer* server = nullptr;
std::string api_key = "benchmark_key";
int port = 8081;

void run_job_submission_benchmark(int num_threads, int requests_per_thread) {
    std::cout << "Starting Job Submission Benchmark: " << num_threads << " threads, " << requests_per_thread << " requests each." << std::endl;

    std::atomic<int> success_count{0};
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            httplib::Client client("localhost", port);
            client.set_connection_timeout(30); // Increased timeout

            for (int j = 0; j < requests_per_thread; ++j) {
                nlohmann::json job_payload = {
                    {"source_url", "http://example.com/video_" + std::to_string(i) + "_" + std::to_string(j) + ".mp4"},
                    {"target_codec", "h264"}
                };

                auto res = client.Post("/jobs/",
                    {{"X-API-Key", api_key}},
                    job_payload.dump(),
                    "application/json");

                if (res && (res->status == 200 || res->status == 201)) {
                    success_count++;
                } else {
                    // std::cerr << "Job failed: " << (res ? res->status : -1) << std::endl;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Job Submission Benchmark Completed." << std::endl;
    std::cout << "Total Requests: " << (num_threads * requests_per_thread) << std::endl;
    std::cout << "Successful Requests: " << success_count << std::endl;
    std::cout << "Time Taken: " << duration << " ms" << std::endl;
    std::cout << "Throughput: " << (double)success_count / (duration / 1000.0) << " req/sec" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}

void run_engine_heartbeat_benchmark(int num_threads, int requests_per_thread) {
    std::cout << "Starting Engine Heartbeat Benchmark: " << num_threads << " threads, " << requests_per_thread << " requests each." << std::endl;

    std::atomic<int> success_count{0};
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            httplib::Client client("localhost", port);
            client.set_connection_timeout(30);

            for (int j = 0; j < requests_per_thread; ++j) {
                nlohmann::json engine_payload = {
                    {"engine_id", "engine_" + std::to_string(i)},
                    {"status", "idle"}
                };

                auto res = client.Post("/engines/heartbeat",
                    {{"X-API-Key", api_key}},
                    engine_payload.dump(),
                    "application/json");

                if (res && res->status == 200) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Engine Heartbeat Benchmark Completed." << std::endl;
    std::cout << "Total Requests: " << (num_threads * requests_per_thread) << std::endl;
    std::cout << "Successful Requests: " << success_count << std::endl;
    std::cout << "Time Taken: " << duration << " ms" << std::endl;
    std::cout << "Throughput: " << (double)success_count / (duration / 1000.0) << " req/sec" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}

void run_concurrent_benchmark(int num_job_threads, int job_requests_per_thread, int num_engine_threads, int engine_requests_per_thread) {
    std::cout << "Starting Concurrent Benchmark (Jobs + Engines)..." << std::endl;

    std::atomic<int> job_success_count{0};
    std::atomic<int> engine_success_count{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;

    // Job threads
    for (int i = 0; i < num_job_threads; ++i) {
        threads.emplace_back([&, i]() {
            httplib::Client client("localhost", port);
            client.set_connection_timeout(30);

            for (int j = 0; j < job_requests_per_thread; ++j) {
                nlohmann::json job_payload = {
                    {"source_url", "http://example.com/video_" + std::to_string(i) + "_" + std::to_string(j) + ".mp4"},
                    {"target_codec", "h264"}
                };

                auto res = client.Post("/jobs/",
                    {{"X-API-Key", api_key}},
                    job_payload.dump(),
                    "application/json");

                if (res && (res->status == 200 || res->status == 201)) {
                    job_success_count++;
                }
            }
        });
    }

    // Engine threads
    for (int i = 0; i < num_engine_threads; ++i) {
        threads.emplace_back([&, i]() {
            httplib::Client client("localhost", port);
            client.set_connection_timeout(30);

            for (int j = 0; j < engine_requests_per_thread; ++j) {
                nlohmann::json engine_payload = {
                    {"engine_id", "engine_conc_" + std::to_string(i)},
                    {"status", "idle"}
                };

                auto res = client.Post("/engines/heartbeat",
                    {{"X-API-Key", api_key}},
                    engine_payload.dump(),
                    "application/json");

                if (res && res->status == 200) {
                    engine_success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Concurrent Benchmark Completed." << std::endl;
    std::cout << "Total Requests: " << (num_job_threads * job_requests_per_thread + num_engine_threads * engine_requests_per_thread) << std::endl;
    std::cout << "Successful Job Requests: " << job_success_count << std::endl;
    std::cout << "Successful Engine Requests: " << engine_success_count << std::endl;
    std::cout << "Time Taken: " << duration << " ms" << std::endl;
    std::cout << "Total Throughput: " << (double)(job_success_count + engine_success_count) / (duration / 1000.0) << " req/sec" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}

int main() {
    // Disable I/O to focus on Mutex Contention
    mock_save_state_enabled = true;
    mock_load_state_enabled = true;

    // Start Server
    server = new DispatchServer();
    server->set_api_key(api_key);
    server->start(port, false); // Non-blocking

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Run Benchmarks
    // Increased concurrency to stress locks
    // run_job_submission_benchmark(20, 100);
    // run_engine_heartbeat_benchmark(20, 100);
    run_concurrent_benchmark(20, 100, 20, 100);

    // Stop Server
    server->stop();
    delete server;

    return 0;
}
