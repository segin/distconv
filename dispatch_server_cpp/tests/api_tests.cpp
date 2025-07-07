#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream> // Required for std::ofstream
#include <cstdio> // Required for std::remove
#include <thread>
#include <chrono>

#include "test_common.h" // For ApiTest fixture and clear_db()

// Test fixture for API tests
class ApiTest : public ::testing::Test {
protected:
    httplib::Client cli;
    ApiTest() : cli("localhost", 8080) {}

    void SetUp() override {
        // Clear the database before each test
        clear_db();
    }
};

TEST_F(ApiTest, JsonParsingValidJobSubmissionRequest) {
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    ASSERT_TRUE(nlohmann::json::parse(res->body).contains("job_id"));
}

TEST_F(ApiTest, JsonParsingValidHeartbeatRequest) {
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
}
