#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h" // For ApiTest fixture and clear_db()
#include <thread>
#include <chrono>

TEST_F(ApiTest, GetJobStatusValid) {
    // First, submit a job to have something to query
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto post_res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(post_res->status, 200);
    nlohmann::json post_response_json = nlohmann::json::parse(post_res->body);
    std::string job_id = post_response_json["job_id"];

    // Now, get the job status
    auto get_res = client->Get(("/jobs/" + job_id).c_str(), headers);

    ASSERT_TRUE(get_res != nullptr);
    ASSERT_EQ(get_res->status, 200);

    nlohmann::json get_response_json = nlohmann::json::parse(get_res->body);
    ASSERT_EQ(get_response_json["job_id"], job_id);
    ASSERT_EQ(get_response_json["status"], "pending");
}

TEST_F(ApiTest, GetJobStatusInvalid) {
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Get("/jobs/invalid_job_id", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404);
    ASSERT_EQ(res->body, "Job not found");
}

TEST_F(ApiTest, GetJobStatusMalformedId) {
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Get("/jobs/this-is-not-a-valid-id", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404);
}

TEST_F(ApiTest, GetJobStatusNoApiKey) {
    httplib::Headers headers = {
        {"Authorization", "some_token"}
    };
    auto res = client->Get("/jobs/some_id", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
}

TEST_F(ApiTest, GetJobStatusIncorrectApiKey) {
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Get("/jobs/some_id", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
}

TEST_F(ApiTest, ListAllJobsEmpty) {
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Get("/jobs/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_TRUE(response_json.empty());
}

TEST_F(ApiTest, ListAllJobsWithJobs) {
    clear_db();
    // Submit two jobs
    nlohmann::json job1_payload;
    job1_payload["source_url"] = "http://example.com/video1.mp4";
    job1_payload["target_codec"] = "h264";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    client->Post("/jobs/", headers, job1_payload.dump(), "application/json");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    nlohmann::json job2_payload;
    job2_payload["source_url"] = "http://example.com/video2.mp4";
    job2_payload["target_codec"] = "vp9";
    client->Post("/jobs/", headers, job2_payload.dump(), "application/json");

    auto res = client->Get("/jobs/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 2);

    // Check some details of the returned jobs
    ASSERT_EQ(response_json[0]["source_url"], "http://example.com/video1.mp4");
    ASSERT_EQ(response_json[1]["source_url"], "http://example.com/video2.mp4");
}

TEST_F(ApiTest, ListAllJobsWithOneJob) {
    // Submit one job
    nlohmann::json job1_payload;
    job1_payload["source_url"] = "http://example.com/video1.mp4";
    job1_payload["target_codec"] = "h264";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    client->Post("/jobs/", headers, job1_payload.dump(), "application/json");

    auto res = client->Get("/jobs/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 1);

    // Check some details of the returned jobs
    ASSERT_EQ(response_json[0]["source_url"], "http://example.com/video1.mp4");
}
