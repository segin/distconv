#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream> // Required for std::ofstream

// Helper function to clear the database before each test
void clear_db() {
    jobs_db = nlohmann::json::object();
    engines_db = nlohmann::json::object();
    // Also clear the persistent storage file
    std::ofstream ofs(PERSISTENT_STORAGE_FILE, std::ios::trunc);
    ofs.close();
}

// Test fixture for API tests
class ApiTest : public ::testing::Test {
protected:
    DispatchServer server;
    httplib::Client *client;
    int port = 8080; // Default port for tests

    void SetUp() override {
        clear_db();
        // Start the server in a non-blocking way
        server.start(port, false);
        client = new httplib::Client("localhost", port);
    }

    void TearDown() override {
        server.stop();
        delete client;
    }
};

TEST_F(ApiTest, SubmitValidJob) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    auto res = client->Post("/jobs/", job_payload.dump(), "application/json");

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
    ASSERT_TRUE(jobs_db.contains(response_json["job_id"]));
}

TEST_F(ApiTest, SubmitJobMissingSourceUrl) {
    nlohmann::json job_payload;
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    auto res = client->Post("/jobs/", job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'source_url' is missing or not a string.");
}

TEST_F(ApiTest, SubmitJobMissingTargetCodec) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    auto res = client->Post("/jobs/", job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'target_codec' is missing or not a string.");
}

TEST_F(ApiTest, SubmitJobInvalidJson) {
    std::string invalid_json_payload = "{this is not json}";

    auto res = client->Post("/jobs/", invalid_json_payload, "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.rfind("Invalid JSON:", 0) == 0);
}

TEST_F(ApiTest, SubmitJobEmptyJson) {
    std::string empty_json_payload = "{}";

    auto res = client->Post("/jobs/", empty_json_payload, "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'source_url' is missing or not a string.");
}

TEST_F(ApiTest, SubmitJobWithExtraFields) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;
    job_payload["extra_field"] = "some_value";
    job_payload["another_extra"] = 123;

    auto res = client->Post("/jobs/", job_payload.dump(), "application/json");

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
    ASSERT_FALSE(jobs_db[response_json["job_id"]].contains("extra_field"));
    ASSERT_FALSE(jobs_db[response_json["job_id"]].contains("another_extra"));
}

TEST_F(ApiTest, SubmitJobNonStringSourceUrl) {
    nlohmann::json job_payload;
    job_payload["source_url"] = 123; // Non-string source_url
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    auto res = client->Post("/jobs/", job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'source_url' is missing or not a string.");
}
