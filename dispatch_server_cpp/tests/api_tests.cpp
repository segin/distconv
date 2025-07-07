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

TEST_F(ApiTest, JsonParsingValidJobCompletionRequest) {
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

    // 4. Mark the job as complete
    nlohmann::json complete_payload = {
        {"output_url", "http://example.com/video_transcoded.mp4"}
    };
    auto res_complete = client->Post("/jobs/" + job_id + "/complete", admin_headers, complete_payload.dump(), "application/json");
    ASSERT_TRUE(res_complete != nullptr);
    ASSERT_EQ(res_complete->status, 200);
}

TEST_F(ApiTest, JsonParsingValidJobFailureRequest) {
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
    ASSERT_TRUE(res_fail != nullptr);
    ASSERT_EQ(res_fail->status, 200);
}

TEST_F(ApiTest, ServerHandlesClientDisconnectingMidRequest) {
    // This test is difficult to simulate precisely with httplib's client, as it handles
    // connection management internally. Instead, we'll simulate an abrupt client close
    // and verify the server remains responsive for subsequent requests.

    // 1. Start a server instance in a separate thread
    DispatchServer server;
    server.set_api_key(api_key);
    std::thread server_thread([&server]() {
        server.start(8080, true); // Blocking call
    });

    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 2. Create a client and make a request, then immediately close the connection
    //    This simulates a client disconnecting mid-request (or very early).
    {
        httplib::Client temp_client("localhost", 8080);
        temp_client.set_connection_timeout(1); // Short timeout
        nlohmann::json job_payload = {
            {"source_url", "http://example.com/video.mp4"},
            {"target_codec", "h264"}
        };
        httplib::Headers admin_headers = {
            {"Authorization", "some_token"},
            {"X-API-Key", api_key}
        };
        // Attempt to post, but the client will close immediately
        temp_client.Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    }

    // Give the server a moment to process the abrupt disconnect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 3. Verify the server is still responsive by making a new request
    nlohmann::json job_payload_2 = {
        {"source_url", "http://example.com/video2.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers_2 = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_2 = client->Post("/jobs/", admin_headers_2, job_payload_2.dump(), "application/json");
    ASSERT_TRUE(res_2 != nullptr);
    ASSERT_EQ(res_2->status, 200);
    ASSERT_TRUE(nlohmann::json::parse(res_2->body).contains("job_id"));

    // 4. Stop the server thread
    server.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

TEST_F(ApiTest, ServerHandlesExtremelyLongSourceUrl) {
    // Create an extremely long source_url
    std::string long_url_base = "http://example.com/long_video_path/";
    std::string long_url_segment(3000, 'a'); // 3000 'a' characters
    std::string extremely_long_source_url = long_url_base + long_url_segment + ".mp4";

    nlohmann::json job_payload = {
        {"source_url", extremely_long_source_url},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    auto res = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200); // Expect 200 OK if the server handles it gracefully

    // Verify the job was stored with the correct long URL
    std::string job_id = nlohmann::json::parse(res->body)["job_id"];
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.contains(job_id));
        ASSERT_EQ(jobs_db[job_id]["source_url"], extremely_long_source_url);
    }
}

TEST_F(ApiTest, ServerHandlesExtremelyLongEngineId) {
    // Create an extremely long engine_id
    std::string extremely_long_engine_id(2000, 'b'); // 2000 'b' characters

    nlohmann::json engine_payload = {
        {"engine_id", extremely_long_engine_id},
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
    ASSERT_EQ(res->status, 200); // Expect 200 OK if the server handles it gracefully

    // Verify the engine was stored with the correct long ID
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(engines_db.contains(extremely_long_engine_id));
        ASSERT_EQ(engines_db[extremely_long_engine_id]["engine_type"], "transcoder");
    }
}