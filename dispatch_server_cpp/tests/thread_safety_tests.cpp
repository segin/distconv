#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h"
#include <vector>
#include <future>

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
