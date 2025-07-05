#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>

#include "test_common.h" 

TEST_F(ApiTest, GetJobStatusValid) {
    // Create a dummy job for testing
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res_post->body);
    std::string job_id = response_json["job_id"];

    // Verify the status update
    auto res_get = client->Get(("/jobs/" + job_id).c_str(), headers);
    ASSERT_TRUE(res_get != nullptr);
    ASSERT_EQ(res_get->status, 200);
    nlohmann::json status_json = nlohmann::json::parse(res_get->body);
    ASSERT_EQ(status_json["status"], "pending");
}

TEST_F(ApiTest, GetJobStatusInvalid) {
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Get("/jobs/invalid_job_id", headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404); // Not Found
}

TEST_F(ApiTest, GetJobStatusMalformedId) {
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Get("/jobs/malformed-id-!@#$", headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404); 
}

TEST_F(ApiTest, GetJobStatusNoApiKey) {
    // Create a dummy job for testing
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res_post->body);
    std::string job_id = response_json["job_id"];

    // Try to get job status without API key
    httplib::Headers no_api_key_headers = {
        {"Authorization", "some_token"}
    };
    auto res = client->Get(("/jobs/" + job_id).c_str(), no_api_key_headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401); // Unauthorized
}

TEST_F(ApiTest, GetJobStatusIncorrectApiKey) {
    // Create a dummy job for testing
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res_post->body);
    std::string job_id = response_json["job_id"];

    // Try to get job status with incorrect API key
    httplib::Headers incorrect_api_key_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", "wrong_api_key"}
    };
    auto res = client->Get(("/jobs/" + job_id).c_str(), incorrect_api_key_headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401); // Unauthorized
}

TEST_F(ApiTest, ListAllJobsEmpty) {
    httplib::Headers headers = {
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
    // Create some dummy jobs
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    for (int i = 0; i < 3; ++i) {
        nlohmann::json request_body;
        request_body["source_url"] = "http://example.com/video.mp4";
        request_body["target_codec"] = "h264";
        auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
        ASSERT_TRUE(res_post != nullptr);
        ASSERT_EQ(res_post->status, 200);
    }

    auto res = client->Get("/jobs/", headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 3);
}

TEST_F(ApiTest, ListAllJobsWithOneJob) {
    // Create a dummy job
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";
    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    auto res = client->Get("/jobs/", headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 1);
}
