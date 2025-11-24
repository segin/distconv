#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h" // For ApiTest fixture and clear_db()
#include <thread>
#include <chrono>

using namespace distconv::DispatchServer;

TEST_F(ApiTest, SubmitValidJob) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.contains("job_id"));
    ASSERT_EQ(response_json["source_url"], "http://example.com/video.mp4");
    ASSERT_EQ(response_json["target_codec"], "h264");
    ASSERT_EQ(response_json["job_size"], 100.5);
    ASSERT_EQ(response_json["status"], "pending");
    ASSERT_EQ(response_json["max_retries"], 5);

    // Verify job is in the database
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.contains(response_json["job_id"]));
    }
}

TEST_F(ApiTest, SubmitJobMissingSourceUrl) {
    nlohmann::json job_payload;
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'source_url' is missing or not a string.");
}

TEST_F(ApiTest, SubmitJobMissingTargetCodec) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'target_codec' is missing or not a string.");
}

TEST_F(ApiTest, SubmitJobInvalidJson) {
    std::string invalid_json_payload = "{this is not json}";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, invalid_json_payload, "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.rfind("Invalid JSON:", 0) == 0);
}

TEST_F(ApiTest, SubmitJobEmptyJson) {
    std::string empty_json_payload = "";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, empty_json_payload, "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
}

TEST_F(ApiTest, SubmitJobWithExtraFields) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;
    job_payload["extra_field"] = "some_value";
    job_payload["another_extra"] = 123;

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.contains("job_id"));
    ASSERT_EQ(response_json["source_url"], "http://example.com/video.mp4");
    ASSERT_EQ(response_json["target_codec"], "h264");
    ASSERT_EQ(response_json["job_size"], 100.5);
    ASSERT_EQ(response_json["status"], "pending");
    ASSERT_EQ(response_json["max_retries"], 5);

    // Verify extra fields are NOT present in the response or in the stored job
    ASSERT_FALSE(response_json.contains("extra_field"));
    ASSERT_FALSE(response_json.contains("another_extra"));
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_FALSE(jobs_db[response_json["job_id"]].contains("extra_field"));
        ASSERT_FALSE(jobs_db[response_json["job_id"]].contains("another_extra"));
    }
}

TEST_F(ApiTest, SubmitJobNoAuthHeader) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";

    httplib::Headers headers;
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
    ASSERT_EQ(res->body, "Unauthorized: Missing 'X-API-Key' header.");
}

TEST_F(ApiTest, SubmitJobNoApiKey) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";

    httplib::Headers headers;
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
    ASSERT_EQ(res->body, "Unauthorized: Missing 'X-API-Key' header.");
}

TEST_F(ApiTest, SubmitJobIncorrectApiKey) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";

    httplib::Headers headers = {
        {"X-API-Key", "incorrect_api_key"}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
    ASSERT_EQ(res->body, "Unauthorized");
}

TEST_F(ApiTest, JobIdIsUnique) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res1 = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto res2 = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res1 != nullptr);
    ASSERT_EQ(res1->status, 200);
    ASSERT_TRUE(res2 != nullptr);
    ASSERT_EQ(res2->status, 200);

    nlohmann::json res1_json = nlohmann::json::parse(res1->body);
    nlohmann::json res2_json = nlohmann::json::parse(res2->body);

    ASSERT_NE(res1_json["job_id"], res2_json["job_id"]);
}

TEST_F(ApiTest, SubmitJobNonStringSourceUrl) {
    nlohmann::json job_payload;
    job_payload["source_url"] = 123; // Non-string source_url
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'source_url' is missing or not a string.");
}

TEST_F(ApiTest, SubmitJobNonStringTargetCodec) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = 123; // Non-string target_codec
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'target_codec' is missing or not a string.");
}

TEST_F(ApiTest, SubmitJobNonNumericJobSize) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = "not_a_number"; // Non-numeric job_size
    job_payload["max_retries"] = 5;

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'job_size' must be a number.");
}

TEST_F(ApiTest, SubmitJobNonIntegerMaxRetries) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 3.5; // Non-integer max_retries

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'max_retries' must be an integer.");
}
