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

TEST_F(ApiTest, ServerHandlesLargeNumberOfJobs) {
    const int num_jobs = 1000; // Number of jobs to create
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    for (int i = 0; i < num_jobs; ++i) {
        nlohmann::json job_payload = {
            {"source_url", "http://example.com/video_large_job_test_" + std::to_string(i) + ".mp4"},
            {"target_codec", "h264"}
        };
        auto res = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
        ASSERT_TRUE(res != nullptr);
        ASSERT_EQ(res->status, 200);
    }

    // Verify that all jobs are present in the database
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_EQ(jobs_db.size(), num_jobs);
    }

    // Optionally, try to retrieve all jobs to ensure the server can handle listing them
    auto res_list = client->Get("/jobs/", admin_headers);
    ASSERT_TRUE(res_list != nullptr);
    ASSERT_EQ(res_list->status, 200);
    nlohmann::json listed_jobs = nlohmann::json::parse(res_list->body);
    ASSERT_EQ(listed_jobs.size(), num_jobs);
}

TEST_F(ApiTest, ServerHandlesLargeNumberOfEngines) {
    const int num_engines = 1000; // Number of engines to create
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    for (int i = 0; i < num_engines; ++i) {
        nlohmann::json engine_payload = {
            {"engine_id", "engine-" + std::to_string(i)},
            {"engine_type", "transcoder"},
            {"supported_codecs", {"h264", "vp9"}},
            {"status", "idle"},
            {"benchmark_time", 100.0}
        };
        auto res = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
        ASSERT_TRUE(res != nullptr);
        ASSERT_EQ(res->status, 200);
    }

    // Verify that all engines are present in the database
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_EQ(engines_db.size(), num_engines);
    }

    // Optionally, try to retrieve all engines to ensure the server can handle listing them
    auto res_list = client->Get("/engines/", admin_headers);
    ASSERT_TRUE(res_list != nullptr);
    ASSERT_EQ(res_list->status, 200);
    nlohmann::json listed_engines = nlohmann::json::parse(res_list->body);
    ASSERT_EQ(listed_engines.size(), num_engines);
}

TEST_F(ApiTest, ServerHandlesNumericJobIdAsString) {
    // Submit a job with a job ID that looks like a number but is a string
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video_numeric_id.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_TRUE(res_submit != nullptr);
    ASSERT_EQ(res_submit->status, 200);
    
    // Extract the job_id from the response, which should be a string
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // Verify that the job_id is indeed a string and not converted to a number
    // The current implementation generates job IDs as strings from milliseconds, so this should pass.
    // If the implementation changes to generate numeric IDs, this test might need adjustment.
    ASSERT_TRUE(job_id.length() > 0);
    ASSERT_FALSE(job_id.empty());

    // Attempt to retrieve the job using the string job ID
    auto res_get = client->Get("/jobs/" + job_id, admin_headers);
    ASSERT_TRUE(res_get != nullptr);
    ASSERT_EQ(res_get->status, 200);
    nlohmann::json retrieved_job = nlohmann::json::parse(res_get->body);
    ASSERT_EQ(retrieved_job["job_id"], job_id);
    ASSERT_EQ(retrieved_job["source_url"], "http://example.com/video_numeric_id.mp4");
}

TEST_F(ApiTest, ServerHandlesHeartbeatForNonExistentAssignedJob) {
    // 1. Create a job and an engine
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video_temp.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    nlohmann::json engine_payload = {
        {"engine_id", "engine-non-existent-job"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 2. Assign the job to the engine
    nlohmann::json assign_payload = {
        {"engine_id", "engine-non-existent-job"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 3. Simulate the job no longer existing by removing it from jobs_db
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db.erase(job_id);
    }

    // 4. Send a heartbeat from the engine (which was assigned the now non-existent job)
    nlohmann::json heartbeat_payload_after_job_removal = {
        {"engine_id", "engine-non-existent-job"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "busy"}, // Engine would still think it's busy
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat_after_removal = client->Post("/engines/heartbeat", admin_headers, heartbeat_payload_after_job_removal.dump(), "application/json");
    ASSERT_TRUE(res_heartbeat_after_removal != nullptr);
    ASSERT_EQ(res_heartbeat_after_removal->status, 200); // Server should still respond OK

    // 5. Verify that the engine still exists in the engines_db and its status is updated
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(engines_db.contains("engine-non-existent-job"));
        ASSERT_EQ(engines_db["engine-non-existent-job"]["status"], "busy"); // Status should be updated from the heartbeat
    }
}

TEST_F(ApiTest, ServerHandlesCompleteJobNeverAssigned) {
    // Attempt to complete a job that was never assigned (i.e., doesn't exist)
    std::string non_existent_job_id = "non_existent_job_123";
    nlohmann::json complete_payload = {
        {"output_url", "http://example.com/output.mp4"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    auto res = client->Post("/jobs/" + non_existent_job_id + "/complete", admin_headers, complete_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404);
    ASSERT_EQ(res->body, "Job not found");
}

TEST_F(ApiTest, ServerHandlesFailJobNeverAssigned) {
    // Attempt to fail a job that was never assigned (i.e., doesn't exist)
    std::string non_existent_job_id = "non_existent_job_456";
    nlohmann::json fail_payload = {
        {"error_message", "Simulated failure"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    auto res = client->Post("/jobs/" + non_existent_job_id + "/fail", admin_headers, fail_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404);
    ASSERT_EQ(res->body, "Job not found");
}

TEST_F(ApiTest, ServerHandlesJobSubmissionWithNegativeJobSize) {
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"job_size", -10.0} // Negative job_size
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400); // Expect Bad Request
    ASSERT_EQ(res->body, "Bad Request: 'job_size' must be a non-negative number.");
}

TEST_F(ApiTest, ServerHandlesJobSubmissionWithNegativeMaxRetries) {
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", -1} // Negative max_retries
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400); // Expect Bad Request
    ASSERT_EQ(res->body, "Bad Request: 'max_retries' must be a non-negative integer.");
}

TEST_F(ApiTest, ServerHandlesHeartbeatWithNegativeStorageCapacityGb) {
    nlohmann::json engine_payload = {
        {"engine_id", "engine-negative-storage"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0},
        {"storage_capacity_gb", -50.0} // Negative storage_capacity_gb
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400); // Expect Bad Request
    ASSERT_EQ(res->body, "Bad Request: 'storage_capacity_gb' must be a non-negative number.");
}

TEST_F(ApiTest, ServerHandlesHeartbeatWithNonBooleanStreamingSupport) {
    nlohmann::json engine_payload = {
        {"engine_id", "engine-non-boolean-streaming"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0},
        {"streaming_support", "true"} // Non-boolean value
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400); // Expect Bad Request
    ASSERT_EQ(res->body, "Bad Request: 'streaming_support' must be a boolean.");
}

TEST_F(ApiTest, ServerHandlesBenchmarkResultForNonExistentEngine) {
    std::string non_existent_engine_id = "non-existent-engine-123";
    nlohmann::json benchmark_payload = {
        {"engine_id", non_existent_engine_id},
        {"benchmark_time", 50.0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    auto res = client->Post("/engines/benchmark_result", admin_headers, benchmark_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404);
    ASSERT_EQ(res->body, "Engine not found");
}

TEST_F(ApiTest, ServerHandlesBenchmarkResultWithNegativeBenchmarkTime) {
    nlohmann::json benchmark_payload = {
        {"engine_id", "engine-123"},
        {"benchmark_time", -50.0} // Negative benchmark_time
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    // First, register the engine so it exists
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    auto res = client->Post("/engines/benchmark_result", admin_headers, benchmark_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400); // Expect Bad Request
    ASSERT_EQ(res->body, "Bad Request: 'benchmark_time' must be a non-negative number.");
}

TEST_F(ApiTest, ServerHandlesJobsTrailingSlash) {
    // Submit a job to /jobs/ with a trailing slash
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video_trailing_slash.mp4"},
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

    // Verify that the job was created
    std::string job_id = nlohmann::json::parse(res->body)["job_id"];
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.contains(job_id));
    }
}

TEST_F(ApiTest, ServerHandlesEnginesTrailingSlash) {
    // Register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine-trailing-slash"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // Request all engines with a trailing slash
    auto res_list = client->Get("/engines/", admin_headers);
    ASSERT_TRUE(res_list != nullptr);
    ASSERT_EQ(res_list->status, 200);
    nlohmann::json listed_engines = nlohmann::json::parse(res_list->body);
    ASSERT_FALSE(listed_engines.empty());
    ASSERT_EQ(listed_engines[0]["engine_id"], "engine-trailing-slash");
}

TEST_F(ApiTest, ServerHandlesNonStandardContentTypeHeader) {
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    // Send a request with a non-standard Content-Type header
    auto res = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/x-custom-json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400); // Expect Bad Request
    ASSERT_EQ(res->body, "Invalid JSON: parse error at 1: syntax error - unexpected ',' in parse_json");
}
