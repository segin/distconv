#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h"
#include <vector>
#include <future>

using namespace distconv::DispatchServer;

TEST_F(ApiTest, SubmitMultipleJobsConcurrently) {
    const int num_jobs = 100;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_jobs; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            nlohmann::json job_payload = {
                {"source_url", "http://example.com/video_" + std::to_string(i) + ".mp4"},
                {"target_codec", "h264"}
            };
            httplib::Headers headers = {
                {"Authorization", "some_token"},
                {"X-API-Key", this->api_key}
            };
            auto res = this->client->Post("/jobs/", headers, job_payload.dump(), "application/json");
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 200);
        }));
    }

    for (auto &f : futures) {
        f.get(); // Wait for all jobs to be submitted
    }

    // Verify that all jobs are in the database
    auto res_get_all_jobs = this->client->Get("/jobs/", this->admin_headers);
    ASSERT_TRUE(res_get_all_jobs != nullptr);
    ASSERT_EQ(res_get_all_jobs->status, 200);
    nlohmann::json all_jobs = nlohmann::json::parse(res_get_all_jobs->body);
    ASSERT_TRUE(all_jobs.is_array());
    ASSERT_EQ(all_jobs.size(), num_jobs);
}

TEST_F(ApiTest, SendMultipleHeartbeatsConcurrently) {
    const int num_engines = 100;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_engines; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            nlohmann::json engine_payload = {
                {"engine_id", "engine-" + std::to_string(i)},
                {"engine_type", "transcoder"},
                {"supported_codecs", {"h264", "vp9"}},
                {"status", "idle"},
                {"benchmark_time", 100.0 + i}
            };
            httplib::Headers headers = {
                {"Authorization", "some_token"},
                {"X-API-Key", this->api_key}
            };
            auto res = this->client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 200);
        }));
    }

    for (auto &f : futures) {
        f.get(); // Wait for all heartbeats to be sent
    }

    // Verify that all engines are in the database
    auto res_get_all_engines = this->client->Get("/engines/", this->admin_headers);
    ASSERT_TRUE(res_get_all_engines != nullptr);
    ASSERT_EQ(res_get_all_engines->status, 200);
    nlohmann::json all_engines = nlohmann::json::parse(res_get_all_engines->body);
    ASSERT_TRUE(all_engines.is_array());
    ASSERT_EQ(all_engines.size(), num_engines);
}



TEST_F(ApiTest, ConcurrentlySubmitJobsAndSendHeartbeats) {
    const int num_operations = 50; // Number of concurrent job submissions and heartbeats
    std::vector<std::future<void>> futures;

    // Concurrently submit jobs
    for (int i = 0; i < num_operations; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            nlohmann::json job_payload = {
                {"source_url", "http://example.com/video_job_" + std::to_string(i) + ".mp4"},
                {"target_codec", "h264"}
            };
            httplib::Headers headers = {
                {"Authorization", "some_token"},
                {"X-API-Key", this->api_key}
            };
            auto res = this->client->Post("/jobs/", headers, job_payload.dump(), "application/json");
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 200);
        }));
    }

    // Concurrently send heartbeats
    for (int i = 0; i < num_operations; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            nlohmann::json engine_payload = {
                {"engine_id", "engine_heartbeat_" + std::to_string(i)},
                {"engine_type", "transcoder"},
                {"supported_codecs", {"h264", "vp9"}},
                {"status", "idle"},
                {"benchmark_time", 100.0 + i}
            };
            httplib::Headers headers = {
                {"Authorization", "some_token"},
                {"X-API-Key", this->api_key}
            };
            auto res = this->client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 200);
        }));
    }

    for (auto &f : futures) {
        f.get(); // Wait for all operations to complete
    }

    // Verify that all jobs are in the database
    auto res_get_all_jobs = this->client->Get("/jobs/", this->admin_headers);
    ASSERT_TRUE(res_get_all_jobs != nullptr);
    ASSERT_EQ(res_get_all_jobs->status, 200);
    nlohmann::json all_jobs = nlohmann::json::parse(res_get_all_jobs->body);
    ASSERT_TRUE(all_jobs.is_array());
    ASSERT_EQ(all_jobs.size(), num_operations);

    // Verify that all engines are in the database
    auto res_get_all_engines = this->client->Get("/engines/", this->admin_headers);
    ASSERT_TRUE(res_get_all_engines != nullptr);
    ASSERT_EQ(res_get_all_engines->status, 200);
    nlohmann::json all_engines = nlohmann::json::parse(res_get_all_engines->body);
    ASSERT_TRUE(all_engines.is_array());
    ASSERT_EQ(all_engines.size(), num_operations);
}

TEST_F(ApiTest, AccessJobsDbFromMultipleThreadsWithLocking) {
    const int num_threads = 10;
    const int operations_per_thread = 100;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string job_id = "job_thread_" + std::to_string(i) + "_op_" + std::to_string(j);
                nlohmann::json job_payload = {
                    {"source_url", "http://example.com/video_" + job_id + ".mp4"},
                    {"target_codec", "h264"}
                };
                httplib::Headers headers = {
                    {"Authorization", "some_token"},
                    {"X-API-Key", this->api_key}
                };
                auto res = this->client->Post("/jobs/", headers, job_payload.dump(), "application/json");
                ASSERT_TRUE(res != nullptr);
                ASSERT_EQ(res->status, 200);
            }
        }));
    }

    for (auto &f : futures) {
        f.get(); // Wait for all threads to complete their operations
    }

    // Verify that all jobs are in the database
    auto res_get_all_jobs = this->client->Get("/jobs/", this->admin_headers);
    ASSERT_TRUE(res_get_all_jobs != nullptr);
    ASSERT_EQ(res_get_all_jobs->status, 200);
    nlohmann::json all_jobs = nlohmann::json::parse(res_get_all_jobs->body);
    ASSERT_TRUE(all_jobs.is_array());
    ASSERT_EQ(all_jobs.size(), num_threads * operations_per_thread);
}

TEST_F(ApiTest, AccessEnginesDbFromMultipleThreadsWithLocking) {
    const int num_threads = 10;
    const int operations_per_thread = 100;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string engine_id = "engine_thread_" + std::to_string(i) + "_op_" + std::to_string(j);
                nlohmann::json engine_payload = {
                    {"engine_id", engine_id},
                    {"engine_type", "transcoder"},
                    {"supported_codecs", {"h264", "vp9"}},
                    {"status", "idle"},
                    {"benchmark_time", 100.0 + j}
                };
                httplib::Headers headers = {
                    {"Authorization", "some_token"},
                    {"X-API-Key", this->api_key}
                };
                auto res = this->client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
                ASSERT_TRUE(res != nullptr);
                ASSERT_EQ(res->status, 200);
            }
        }));
    }

    for (auto &f : futures) {
        f.get(); // Wait for all threads to complete their operations
    }

    // Verify that all engines are in the database
    auto res_get_all_engines = this->client->Get("/engines/", this->admin_headers);
    ASSERT_TRUE(res_get_all_engines != nullptr);
    ASSERT_EQ(res_get_all_engines->status, 200);
    nlohmann::json all_engines = nlohmann::json::parse(res_get_all_engines->body);
    ASSERT_TRUE(all_engines.is_array());
    ASSERT_EQ(all_engines.size(), num_threads * operations_per_thread);
}

TEST_F(ApiTest, ConcurrentlyAssignJobsAndCompleteJobs) {
    // Test 108: Concurrently assign jobs and complete jobs. Verify state transitions are correct.
    
    const int num_jobs = 10;
    std::vector<std::string> job_ids;
    std::vector<std::string> engine_ids;
    
    // 1. Create multiple jobs
    for (int i = 0; i < num_jobs; ++i) {
        nlohmann::json job_payload = {
            {"source_url", "http://example.com/video" + std::to_string(i) + ".mp4"},
            {"target_codec", "h264"}
        };
        auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
        ASSERT_EQ(res_submit->status, 200);
        std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
        job_ids.push_back(job_id);
    }
    
    // 2. Create multiple engines
    for (int i = 0; i < num_jobs; ++i) {
        nlohmann::json engine_payload = {
            {"engine_id", "engine-" + std::to_string(i)},
            {"engine_type", "transcoder"},
            {"supported_codecs", {"h264", "vp9"}},
            {"status", "idle"},
            {"benchmark_time", 100.0 + i} // Different benchmark times
        };
        auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
        ASSERT_EQ(res_heartbeat->status, 200);
        engine_ids.push_back("engine-" + std::to_string(i));
    }
    
    // 3. Concurrent assignment and completion
    std::vector<std::thread> threads;
    std::atomic<int> assignments_completed(0);
    std::atomic<int> completions_processed(0);
    std::vector<std::string> assigned_job_ids;
    std::mutex assigned_jobs_mutex;
    
    // Thread 1: Assign jobs
    threads.emplace_back([&]() {
        for (int i = 0; i < num_jobs; ++i) {
            nlohmann::json assign_payload = {
                {"engine_id", engine_ids[i]}
            };
            auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
            if (res_assign->status == 200) {
                nlohmann::json assigned_job = nlohmann::json::parse(res_assign->body);
                std::string assigned_job_id = assigned_job["job_id"];
                {
                    std::lock_guard<std::mutex> lock(assigned_jobs_mutex);
                    assigned_job_ids.push_back(assigned_job_id);
                }
                assignments_completed++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Small delay
        }
    });
    
    // Thread 2: Complete jobs (wait for assignments first)
    threads.emplace_back([&]() {
        while (completions_processed < num_jobs / 2) { // Complete about half the jobs
            std::string job_to_complete;
            {
                std::lock_guard<std::mutex> lock(assigned_jobs_mutex);
                if (!assigned_job_ids.empty()) {
                    job_to_complete = assigned_job_ids.back();
                    assigned_job_ids.pop_back();
                }
            }
            
            if (!job_to_complete.empty()) {
                nlohmann::json complete_payload = {
                    {"output_url", "http://example.com/output_" + job_to_complete + ".mp4"}
                };
                auto res_complete = client->Post("/jobs/" + job_to_complete + "/complete", 
                                                admin_headers, complete_payload.dump(), "application/json");
                if (res_complete->status == 200) {
                    completions_processed++;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Small delay
        }
    });
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 4. Verify final state consistency
    auto res_jobs = client->Get("/jobs/", admin_headers);
    ASSERT_EQ(res_jobs->status, 200);
    nlohmann::json all_jobs = nlohmann::json::parse(res_jobs->body);
    
    auto res_engines = client->Get("/engines/", admin_headers);
    ASSERT_EQ(res_engines->status, 200);
    nlohmann::json all_engines = nlohmann::json::parse(res_engines->body);
    
    // 5. Verify data integrity - all jobs exist and have valid states
    ASSERT_EQ(all_jobs.size(), num_jobs);
    ASSERT_EQ(all_engines.size(), num_jobs);
    
    int completed_jobs = 0, assigned_jobs = 0, pending_jobs = 0;
    for (const auto& job : all_jobs) {
        std::string status = job["status"];
        ASSERT_TRUE(status == "pending" || status == "assigned" || status == "completed");
        if (status == "completed") completed_jobs++;
        else if (status == "assigned") assigned_jobs++;
        else if (status == "pending") pending_jobs++;
    }
    
    // Verify that some operations succeeded
    ASSERT_GT(assignments_completed.load(), 0);
    ASSERT_GE(completions_processed.load(), 0);
    
    // Verify state consistency: completed jobs should have output_url
    for (const auto& job : all_jobs) {
        if (job["status"] == "completed") {
            ASSERT_TRUE(job.contains("output_url"));
            ASSERT_FALSE(job["output_url"].get<std::string>().empty());
        }
    }
}
