#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h" // For ApiTest fixture and clear_db()

TEST_F(ApiTest, AssignJob) {
    // Submit a job
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    // Send a heartbeat from an engine
    nlohmann::json heartbeat_payload;
    heartbeat_payload["engine_id"] = "engine-123";
    heartbeat_payload["status"] = "idle";
    heartbeat_payload["supported_codecs"] = {"h264", "vp9"};
    heartbeat_payload["benchmark_time"] = 100.0;
    client->Post("/engines/heartbeat", headers, heartbeat_payload.dump(), "application/json");

    // Assign the job
    nlohmann::json assign_payload;
    assign_payload["engine_id"] = "engine-123";
    auto res = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json res_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(res_json.contains("job_id"));
    ASSERT_EQ(res_json["status"], "assigned");
    ASSERT_EQ(res_json["assigned_engine"], "engine-123");
}

TEST_F(ApiTest, JobStatusIsPending) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json res_json = nlohmann::json::parse(res->body);
    std::string job_id = res_json["job_id"];

    // Verify the status in the response
    ASSERT_EQ(res_json["status"], "pending");

    // Verify the status in the database
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        ASSERT_EQ(jobs_db[job_id]["status"], "pending");
    }
}

TEST_F(ApiTest, JobBecomesFailedPermanently) {
    // Submit a job with max_retries = 0
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    job_payload["max_retries"] = 0;
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto post_res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    std::string job_id = nlohmann::json::parse(post_res->body)["job_id"];

    // Fail the job
    nlohmann::json fail_payload;
    fail_payload["error_message"] = "Transcoding failed";
    auto fail_res = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_payload.dump(), "application/json");

    ASSERT_TRUE(fail_res != nullptr);
    ASSERT_EQ(fail_res->status, 200);

    // Verify job status
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        ASSERT_EQ(jobs_db[job_id]["status"], "failed_permanently");
    }
}

TEST_F(ApiTest, CompletedJobCannotBeFailed) {
    // Submit a job first
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto post_res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    std::string job_id = nlohmann::json::parse(post_res->body)["job_id"];

    // Complete the job
    nlohmann::json complete_payload;
    complete_payload["output_url"] = "http://example.com/output.mp4";
    client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, complete_payload.dump(), "application/json");

    // Try to fail the job
    nlohmann::json fail_payload;
    fail_payload["error_message"] = "Transcoding failed";
    auto fail_res = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_payload.dump(), "application/json");

    ASSERT_TRUE(fail_res != nullptr);
    ASSERT_EQ(fail_res->status, 400);

    // Verify job status is still completed
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        ASSERT_EQ(jobs_db[job_id]["status"], "completed");
    }
}



TEST_F(ApiTest, CompletedJobNotAssigned) {
    // Submit and complete a job
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto post_res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    std::string job_id = nlohmann::json::parse(post_res->body)["job_id"];
    nlohmann::json complete_payload;
    complete_payload["output_url"] = "http://example.com/output.mp4";
    client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, complete_payload.dump(), "application/json");

    // Send a heartbeat from an engine
    nlohmann::json heartbeat_payload;
    heartbeat_payload["engine_id"] = "engine-123";
    heartbeat_payload["status"] = "idle";
    heartbeat_payload["supported_codecs"] = {"h264", "vp9"};
    heartbeat_payload["benchmark_time"] = 100.0;
    client->Post("/engines/heartbeat", headers, heartbeat_payload.dump(), "application/json");

    // Try to assign a job
    nlohmann::json assign_payload;
    assign_payload["engine_id"] = "engine-123";
    auto res = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 204);
}