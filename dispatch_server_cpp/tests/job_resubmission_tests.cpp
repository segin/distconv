#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>

#include "test_common.h"

using namespace distconv::DispatchServer;

TEST_F(ApiTest, FailedJobHasRetriesCountIncremented) {
    // 1. Create a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 3. Assign the job to the engine
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 4. Mark the job as failed
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post("/jobs/" + job_id + "/fail", admin_headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);

    // 5. Check the job's status
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["retries"], 1);
}

TEST_F(ApiTest, FailedJobIsRequeued) {
    // 1. Create a job with max_retries > 1
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", 2}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 3. Assign the job to the engine
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 4. Mark the job as failed
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post("/jobs/" + job_id + "/fail", admin_headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);

    // 5. Check the job's status
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["status"], "pending");
}

TEST_F(ApiTest, FailedJobBecomesPermanentlyFailed) {
    // 1. Create a job with max_retries = 1
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", 1}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 3. Assign the job to the engine
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 4. Mark the job as failed
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post("/jobs/" + job_id + "/fail", admin_headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);

    // 5. Check the job's status
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["status"], "failed_permanently");
}

TEST_F(ApiTest, MaxRetriesDefaultsToThree) {
    // 1. Create a job without specifying max_retries
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Check the job's max_retries
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["max_retries"], 3);
}

TEST_F(ApiTest, MaxRetriesCanBeSetToZero) {
    // 1. Create a job with max_retries = 0
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", 0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Check the job's max_retries
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["max_retries"], 0);
}

TEST_F(ApiTest, JobWithZeroMaxRetriesFailsPermanently) {
    // 1. Create a job with max_retries = 0
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", 0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 3. Assign the job to the engine
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 4. Mark the job as failed
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post("/jobs/" + job_id + "/fail", admin_headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);

    // 5. Check the job's status
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["status"], "failed_permanently");
}
