#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream> // Required for std::ofstream
#include <cstdio> // Required for std::remove
#include <thread>
#include <chrono>

#include "test_common.h" // For ApiTest fixture and clear_db()

TEST_F(ApiTest, UpdateJobStatusValid) {
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

    // Update the job status
    nlohmann::json update_body;
    update_body["output_url"] = "http://example.com/video_out.mp4";

    auto res_put = client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, update_body.dump(), "application/json");
    ASSERT_TRUE(res_put != nullptr);
    ASSERT_EQ(res_put->status, 200);

    // Verify the status update
    auto res_get = client->Get(("/jobs/" + job_id).c_str(), headers);
    ASSERT_TRUE(res_get != nullptr);
    ASSERT_EQ(res_get->status, 200);
    nlohmann::json status_json = nlohmann::json::parse(res_get->body);
    ASSERT_EQ(status_json["status"], "completed");
}

TEST_F(ApiTest, UpdateJobStatusInvalidJobId) {
    nlohmann::json update_body;
    update_body["output_url"] = "http://example.com/video_out.mp4";

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    auto res = client->Post("/jobs/invalid_job_id/complete", headers, update_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404); // Not Found
}

TEST_F(ApiTest, UpdateJobStatusMalformedId) {
    nlohmann::json update_body;
    update_body["output_url"] = "http://example.com/video_out.mp4";

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    auto res = client->Post("/jobs/malformed-id-!@#$/complete", headers, update_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404); 
}

TEST_F(ApiTest, UpdateJobStatusNoApiKey) {
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

    // Try to update job status without API key
    nlohmann::json update_body;
    update_body["output_url"] = "http://example.com/video_out.mp4";

    httplib::Headers no_api_key_headers = {
        {"Authorization", "some_token"}
    };

    auto res = client->Post(("/jobs/" + job_id + "/complete").c_str(), no_api_key_headers, update_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401); // Unauthorized
}

TEST_F(ApiTest, UpdateJobStatusIncorrectApiKey) {
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

    // Try to update job status with incorrect API key
    nlohmann::json update_body;
    update_body["output_url"] = "http://example.com/video_out.mp4";

    httplib::Headers incorrect_api_key_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", "wrong_api_key"}
    };

    auto res = client->Post(("/jobs/" + job_id + "/complete").c_str(), incorrect_api_key_headers, update_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401); // Unauthorized
}

TEST_F(ApiTest, UpdateJobStatusInvalidStatus) {
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

    // Try to update job status with invalid status
    nlohmann::json update_body;
    update_body["output_url"] = "http://example.com/video_out.mp4";

    auto res = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, update_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400); // Bad Request
}

TEST_F(ApiTest, CompleteJobMissingOutputUrl) {
    // Create a dummy job for testing
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };

    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res_post->body);
    std::string job_id = response_json["job_id"];

    // Try to complete job without output_url
    nlohmann::json update_body;
    // Missing output_url

    auto res = client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, update_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'output_url' must be a string.");
}

TEST_F(ApiTest, CompleteJobInvalidJson) {
    // Create a dummy job for testing
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };

    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res_post->body);
    std::string job_id = response_json["job_id"];

    // Try to complete job with invalid JSON
    std::string invalid_json_payload = "{this is not json}";

    auto res = client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, invalid_json_payload, "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.rfind("Invalid JSON:", 0) == 0);
}

TEST_F(ApiTest, CompleteJobNoApiKey) {
    // Create a dummy job for testing
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };

    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res_post->body);
    std::string job_id = response_json["job_id"];

    // Try to complete job without API key
    nlohmann::json update_body;
    update_body["output_url"] = "http://example.com/video_out.mp4";

    httplib::Headers no_api_key_headers;

    auto res = client->Post(("/jobs/" + job_id + "/complete").c_str(), no_api_key_headers, update_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
    ASSERT_EQ(res->body, "Unauthorized: Missing 'X-API-Key' header.");
}

TEST_F(ApiTest, FailJobValid) {
    // Create a dummy job for testing
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };

    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res_post->body);
    std::string job_id = response_json["job_id"];

    // Fail the job
    nlohmann::json fail_body;
    fail_body["error_message"] = "Transcoding failed due to codec incompatibility";

    auto res_fail = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_body.dump(), "application/json");
    ASSERT_TRUE(res_fail != nullptr);
    ASSERT_EQ(res_fail->status, 200);
}

TEST_F(ApiTest, FailJobInvalidJobId) {
    nlohmann::json fail_body;
    fail_body["error_message"] = "Transcoding failed";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };

    auto res = client->Post("/jobs/invalid_job_id/fail", headers, fail_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404);
    ASSERT_EQ(res->body, "Job not found");
}

TEST_F(ApiTest, FailJobMissingErrorMessage) {
    // Create a dummy job for testing
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };

    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res_post->body);
    std::string job_id = response_json["job_id"];

    // Try to fail job without error_message
    nlohmann::json fail_body;
    // Missing error_message

    auto res = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'error_message' is missing.");
}

TEST_F(ApiTest, FailJobInvalidJson) {
    // Create a dummy job for testing
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };

    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res_post->body);
    std::string job_id = response_json["job_id"];

    // Try to fail job with invalid JSON
    std::string invalid_json_payload = "{this is not json}";

    auto res = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, invalid_json_payload, "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.rfind("Invalid JSON:", 0) == 0);
}

TEST_F(ApiTest, FailJobNoApiKey) {
    // Create a dummy job for testing
    nlohmann::json request_body;
    request_body["source_url"] = "http://example.com/video.mp4";
    request_body["target_codec"] = "h264";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };

    auto res_post = client->Post("/jobs/", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res_post->body);
    std::string job_id = response_json["job_id"];

    // Try to fail job without API key
    nlohmann::json fail_body;
    fail_body["error_message"] = "Transcoding failed";

    httplib::Headers no_api_key_headers;

    auto res = client->Post(("/jobs/" + job_id + "/fail").c_str(), no_api_key_headers, fail_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
    ASSERT_EQ(res->body, "Unauthorized: Missing 'X-API-Key' header.");
}
