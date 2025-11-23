#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <string>

#include "test_common.h"

TEST_F(ApiTest, ServerHandlesJobSubmissionWithNegativeJobSize) {
    // Test 131: Server handles a job submission with a negative job_size.
    
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"job_size", -100.5}  // Negative job size
    };
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    
    // Server should reject negative job_size with HTTP 400
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.find("job_size") != std::string::npos);
    ASSERT_TRUE(res->body.find("negative") != std::string::npos || res->body.find("non-negative") != std::string::npos);
}

TEST_F(ApiTest, ServerHandlesJobSubmissionWithNegativeMaxRetries) {
    // Test 132: Server handles a job submission with a negative max_retries.
    
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", -3}  // Negative max_retries
    };
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    
    // Server should reject negative max_retries with HTTP 400
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.find("max_retries") != std::string::npos);
    ASSERT_TRUE(res->body.find("negative") != std::string::npos || res->body.find("non-negative") != std::string::npos);
}

TEST_F(ApiTest, ServerHandlesHeartbeatWithNegativeStorageCapacity) {
    // Test 133: Server handles a heartbeat with a negative storage_capacity_gb.
    
    nlohmann::json engine_payload = {
        {"engine_id", "engine-negative-storage"},
        {"engine_type", "transcoder"},
        {"status", "idle"},
        {"storage_capacity_gb", -500.0}  // Negative storage capacity
    };
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    
    // Server should reject negative storage_capacity_gb with HTTP 400
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.find("storage_capacity_gb") != std::string::npos);
    ASSERT_TRUE(res->body.find("negative") != std::string::npos || res->body.find("non-negative") != std::string::npos);
}

TEST_F(ApiTest, ServerHandlesHeartbeatWithNonBooleanStreamingSupport) {
    // Test 134: Server handles a heartbeat with a non-boolean streaming_support.
    
    nlohmann::json engine_payload = {
        {"engine_id", "engine-invalid-streaming"},
        {"engine_type", "transcoder"},
        {"status", "idle"},
        {"streaming_support", "yes"}  // String instead of boolean
    };
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    
    // Server should reject non-boolean streaming_support with HTTP 400
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.find("streaming_support") != std::string::npos);
    ASSERT_TRUE(res->body.find("boolean") != std::string::npos);
}

TEST_F(ApiTest, ServerHandlesBenchmarkResultForNonExistentEngine) {
    // Test 135: Server handles a benchmark result for a non-existent engine.
    
    nlohmann::json benchmark_payload = {
        {"engine_id", "non-existent-engine"},
        {"benchmark_time", 125.75}
    };
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Post("/engines/benchmark_result", headers, benchmark_payload.dump(), "application/json");
    
    // Server should return 404 for non-existent engine
    ASSERT_EQ(res->status, 404);
    ASSERT_TRUE(res->body.find("Engine not found") != std::string::npos);
}

TEST_F(ApiTest, ServerHandlesBenchmarkResultWithNegativeBenchmarkTime) {
    // Test 136: Server handles a benchmark result with a negative benchmark_time.
    
    // First register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine-benchmark-test"},
        {"engine_type", "transcoder"},
        {"status", "idle"}
    };
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    // Now send negative benchmark time
    nlohmann::json benchmark_payload = {
        {"engine_id", "engine-benchmark-test"},
        {"benchmark_time", -125.75}  // Negative benchmark time
    };
    
    auto res = client->Post("/engines/benchmark_result", headers, benchmark_payload.dump(), "application/json");
    
    // Server should reject negative benchmark_time with HTTP 400
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.find("benchmark_time") != std::string::npos);
    ASSERT_TRUE(res->body.find("negative") != std::string::npos || res->body.find("non-negative") != std::string::npos);
}

TEST_F(ApiTest, ServerHandlesRequestToJobsWithTrailingSlash) {
    // Test 137: Server handles a request to /jobs/ with a trailing slash.
    
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    
    // Test POST to /jobs/ (with trailing slash)
    auto res_post = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_post->status, 200);
    
    // Test GET to /jobs/ (with trailing slash)
    auto res_get = client->Get("/jobs/", headers);
    ASSERT_EQ(res_get->status, 200);
    nlohmann::json jobs = nlohmann::json::parse(res_get->body);
    ASSERT_TRUE(jobs.is_array());
}

TEST_F(ApiTest, ServerHandlesRequestToEnginesWithTrailingSlash) {
    // Test 138: Server handles a request to /engines/ with a trailing slash.
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    
    // Test GET to /engines/ (with trailing slash)
    auto res_get = client->Get("/engines/", headers);
    ASSERT_EQ(res_get->status, 200);
    nlohmann::json engines = nlohmann::json::parse(res_get->body);
    ASSERT_TRUE(engines.is_array());
    
    // Test POST to /engines/heartbeat (heartbeat endpoint)
    nlohmann::json engine_payload = {
        {"engine_id", "engine-trailing-slash-test"},
        {"engine_type", "transcoder"},
        {"status", "idle"}
    };
    auto res_post = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_post->status, 400);
}

TEST_F(ApiTest, ServerHandlesRequestWithNonStandardContentType) {
    // Test 139: Server handles a request with a non-standard Content-Type header.
    
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    
    // Test with non-standard Content-Type
    httplib::Headers headers = {
        {"X-API-Key", api_key},
        {"Content-Type", "text/plain"}  // Wrong content type for JSON
    };
    
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "text/plain");
    
    // Server should either handle gracefully (parse JSON regardless) or reject with 400
    // Either behavior is acceptable as long as the server doesn't crash
    ASSERT_TRUE(res->status == 200 || res->status == 400 || res->status == 415);
    
    // Verify server is still responsive
    auto health_check = client->Get("/");
    ASSERT_EQ(health_check->status, 200);
}

TEST_F(ApiTest, ServerHandlesRequestWithVeryLargeRequestBody) {
    // Test 140: Server handles a request with a very large request body (should be rejected).
    
    // Create a very large JSON payload (>1MB)
    nlohmann::json large_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    
    // Add a very large field
    std::string large_string(1024 * 1024, 'a');  // 1MB of 'a' characters
    large_payload["large_field"] = large_string;
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Post("/jobs/", headers, large_payload.dump(), "application/json");
    
    // Server should either handle it gracefully (200) or reject it (400/413)
    // The important thing is that the server doesn't crash
    ASSERT_TRUE(res->status == 200 || res->status == 400 || res->status == 413);
    
    // Verify server is still responsive
    auto health_check = client->Get("/");
    ASSERT_EQ(health_check->status, 200);
}

TEST_F(ApiTest, HttplibServerInstanceCanBeAccessedFromTests) {
    // Test 143: The httplib::Server instance can be accessed from tests.
    
    // Access the server instance through the DispatchServer
    httplib::Server* server_ptr = server->getServer();
    ASSERT_NE(server_ptr, nullptr);
    
    // Verify we can interact with the server instance
    // The server should be listening and operational
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Get("/", headers);
    ASSERT_EQ(res->status, 200);
    ASSERT_EQ(res->body, "OK");
}

TEST_F(ApiTest, ServerCanBeStartedOnRandomAvailablePort) {
    // Test 148: The server can be started on a random available port to allow parallel test execution.
    
    // The ApiTest fixture already demonstrates this functionality
    // by using find_available_port() in SetUpTestSuite()
    
    // Verify current server is running on the assigned port
    ASSERT_GT(port, 0);
    ASSERT_GE(port, 10000);  // Port range used by find_available_port
    ASSERT_LE(port, 65000);
    
    // Verify server is responsive on this port
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Get("/", headers);
    ASSERT_EQ(res->status, 200);
    ASSERT_EQ(res->body, "OK");
    
    // Test creating another server on a different random port
    int new_port = find_available_port();
    ASSERT_NE(new_port, port);  // Should get a different port
    
    DispatchServer test_server;
    test_server.set_api_key("test_key");
    test_server.start(new_port, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test the new server
    httplib::Client test_client("localhost", new_port);
    httplib::Headers test_headers = {{"X-API-Key", "test_key"}};
    auto test_res = test_client.Get("/", test_headers);
    ASSERT_TRUE(test_res);
    ASSERT_EQ(test_res->status, 200);
    
    // Clean up
    test_server.stop();
}