#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream> // Required for std::ofstream
#include <cstdio> // Required for std::remove
#include <thread>
#include <chrono>

// Helper function to clear the database before each test
void clear_db() {
    jobs_db = nlohmann::json::object();
    engines_db = nlohmann::json::object();
    // Also clear the persistent storage file
    std::remove(PERSISTENT_STORAGE_FILE.c_str());
}

// Test fixture for API tests
class ApiTest : public ::testing::Test {
protected:
    DispatchServer *server;
    httplib::Client *client;
    int port = 8080; // Default port for tests
    std::string api_key = "test_api_key";

    void SetUp() override {
        clear_db();
        server = new DispatchServer(api_key);
        // Start the server in a non-blocking way
        server->start(port, false);
        client = new httplib::Client("localhost", port);
    }

    void TearDown() override {
        delete client;
        server->stop();
        delete server;
    }
};





TEST_F(ApiTest, SubmitJobMissingTargetCodec) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    httplib::Headers headers = {
        {"Authorization", "some_token"},
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
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, invalid_json_payload, "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.rfind("Invalid JSON:", 0) == 0);
}

TEST_F(ApiTest, SubmitJobEmptyJson) {
    std::string empty_json_payload = "{}";

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, empty_json_payload, "application/json");

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

    httplib::Headers headers = {
        {"Authorization", "some_token"},
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
        std::lock_guard<std::mutex> lock(jobs_mutex);
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
    ASSERT_EQ(res->body, "Unauthorized: Missing 'Authorization' header.");
}

TEST_F(ApiTest, SubmitJobNoApiKey) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";

    httplib::Headers headers = {
        {"Authorization", "some_token"}
    };
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
        {"Authorization", "some_token"},
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
        {"Authorization", "some_token"},
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
        {"Authorization", "some_token"},
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
        {"Authorization", "some_token"},
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
        {"Authorization", "some_token"},
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
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'max_retries' must be an integer.");
}

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
        {"X-API-Key", "incorrect_api_key"}
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

TEST_F(ApiTest, ListAllJobsNoApiKey) {
    httplib::Headers headers = {
        {"Authorization", "some_token"}
    };
    auto res = client->Get("/jobs/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
}

TEST_F(ApiTest, ListAllJobsIncorrectApiKey) {
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", "incorrect_api_key"}
    };
    auto res = client->Get("/jobs/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
}

TEST_F(ApiTest, EngineHeartbeatNewEngine) {
    nlohmann::json heartbeat_payload;
    heartbeat_payload["engine_id"] = "engine-123";
    heartbeat_payload["status"] = "idle";
    heartbeat_payload["supported_codecs"] = {"h264", "vp9"};

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/engines/heartbeat", headers, heartbeat_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    ASSERT_EQ(res->body, "Heartbeat received from engine engine-123");

    // Verify engine is in the database
    {
        std::lock_guard<std::mutex> lock(engines_mutex);
        ASSERT_TRUE(engines_db.contains("engine-123"));
        ASSERT_EQ(engines_db["engine-123"]["status"], "idle");
    }
}

TEST_F(ApiTest, EngineHeartbeatExistingEngine) {
    // First heartbeat
    nlohmann::json heartbeat_payload1;
    heartbeat_payload1["engine_id"] = "engine-123";
    heartbeat_payload1["status"] = "idle";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    client->Post("/engines/heartbeat", headers, heartbeat_payload1.dump(), "application/json");

    // Second heartbeat, updating status
    nlohmann::json heartbeat_payload2;
    heartbeat_payload2["engine_id"] = "engine-123";
    heartbeat_payload2["status"] = "busy";
    auto res = client->Post("/engines/heartbeat", headers, heartbeat_payload2.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    // Verify engine status is updated
    {
        std::lock_guard<std::mutex> lock(engines_mutex);
        ASSERT_EQ(engines_db["engine-123"]["status"], "busy");
    }
}

TEST_F(ApiTest, ListAllEngines) {
    // Add two engines
    nlohmann::json engine1_payload;
    engine1_payload["engine_id"] = "engine-1";
    engine1_payload["status"] = "idle";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    client->Post("/engines/heartbeat", headers, engine1_payload.dump(), "application/json");

    nlohmann::json engine2_payload;
    engine2_payload["engine_id"] = "engine-2";
    engine2_payload["status"] = "busy";
    client->Post("/engines/heartbeat", headers, engine2_payload.dump(), "application/json");

    auto res = client->Get("/engines/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 2);
    ASSERT_EQ(response_json[0]["engine_id"], "engine-1");
    ASSERT_EQ(response_json[1]["engine_id"], "engine-2");
}

TEST_F(ApiTest, ListAllEnginesWithOneEngine) {
    // Add one engine
    nlohmann::json engine1_payload;
    engine1_payload["engine_id"] = "engine-1";
    engine1_payload["status"] = "idle";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    client->Post("/engines/heartbeat", headers, engine1_payload.dump(), "application/json");

    auto res = client->Get("/engines/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 1);
    ASSERT_EQ(response_json[0]["engine_id"], "engine-1");
}

TEST_F(ApiTest, ListAllEnginesNoApiKey) {
    httplib::Headers headers = {
        {"Authorization", "some_token"}
    };
    auto res = client->Get("/engines/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
}

TEST_F(ApiTest, ListAllEnginesEmpty) {
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Get("/engines/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_TRUE(response_json.empty());
}

TEST_F(ApiTest, ListAllEnginesIncorrectApiKey) {
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", "incorrect_api_key"}
    };
    auto res = client->Get("/engines/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
}

TEST_F(ApiTest, EngineHeartbeatInvalidJson) {
    std::string invalid_json_payload = "{this is not json}";

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/engines/heartbeat", headers, invalid_json_payload, "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_TRUE(res->body.rfind("Invalid JSON:", 0) == 0);
}

TEST_F(ApiTest, EngineHeartbeatMissingEngineId) {
    nlohmann::json heartbeat_payload;
    heartbeat_payload["status"] = "idle";
    heartbeat_payload["supported_codecs"] = {"h264", "vp9"};

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/engines/heartbeat", headers, heartbeat_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'engine_id' is missing.");
}

TEST_F(ApiTest, EngineHeartbeatNonStringEngineId) {
    nlohmann::json heartbeat_payload;
    heartbeat_payload["engine_id"] = 123;
    heartbeat_payload["status"] = "idle";
    heartbeat_payload["supported_codecs"] = {"h264", "vp9"};

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/engines/heartbeat", headers, heartbeat_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 400);
    ASSERT_EQ(res->body, "Bad Request: 'engine_id' must be a string.");
}

TEST_F(ApiTest, EngineHeartbeatNoApiKey) {
    nlohmann::json heartbeat_payload;
    heartbeat_payload["engine_id"] = "engine-123";
    heartbeat_payload["status"] = "idle";
    heartbeat_payload["supported_codecs"] = {"h264", "vp9"};

    httplib::Headers headers = {
        {"Authorization", "some_token"}
    };
    auto res = client->Post("/engines/heartbeat", headers, heartbeat_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
}

TEST_F(ApiTest, EngineHeartbeatIncorrectApiKey) {
    nlohmann::json heartbeat_payload;
    heartbeat_payload["engine_id"] = "engine-123";
    heartbeat_payload["status"] = "idle";
    heartbeat_payload["supported_codecs"] = {"h264", "vp9"};

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", "incorrect_api_key"}
    };
    auto res = client->Post("/engines/heartbeat", headers, heartbeat_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
}

TEST_F(ApiTest, SubmitBenchmarkResultValid) {
    // First, register an engine
    nlohmann::json engine_payload;
    engine_payload["engine_id"] = "engine-bm-1";
    engine_payload["status"] = "benchmarking";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");

    // Now, submit benchmark result
    nlohmann::json benchmark_payload;
    benchmark_payload["engine_id"] = "engine-bm-1";
    benchmark_payload["benchmark_time"] = 123.45;

    auto res = client->Post("/engines/benchmark_result", headers, benchmark_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    ASSERT_EQ(res->body, "Benchmark result received from engine engine-bm-1");

    // Verify benchmark time is stored
    {
        std::lock_guard<std::mutex> lock(engines_mutex);
        ASSERT_EQ(engines_db["engine-bm-1"]["benchmark_time"], 123.45);
    }
}

TEST_F(ApiTest, SubmitBenchmarkResultInvalidEngine) {
    nlohmann::json benchmark_payload;
    benchmark_payload["engine_id"] = "non-existent-engine";
    benchmark_payload["benchmark_time"] = 123.45;

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/engines/benchmark_result", headers, benchmark_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404);
    ASSERT_EQ(res->body, "Engine not found");
}

TEST_F(ApiTest, CompleteJobValid) {
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
    auto complete_res = client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, complete_payload.dump(), "application/json");

    ASSERT_TRUE(complete_res != nullptr);
    ASSERT_EQ(complete_res->status, 200);
    ASSERT_EQ(complete_res->body, "Job " + job_id + " marked as completed");

    // Verify job status
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        ASSERT_EQ(jobs_db[job_id]["status"], "completed");
        ASSERT_EQ(jobs_db[job_id]["output_url"], "http://example.com/output.mp4");
    }
}

TEST_F(ApiTest, CompleteJobInvalid) {
    nlohmann::json complete_payload;
    complete_payload["output_url"] = "http://example.com/output.mp4";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/invalid_job_id/complete", headers, complete_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404);
    ASSERT_EQ(res->body, "Job not found");
}

TEST_F(ApiTest, CompleteJobMissingOutputUrl) {
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
    auto complete_res = client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, complete_payload.dump(), "application/json");

    ASSERT_TRUE(complete_res != nullptr);
    ASSERT_EQ(complete_res->status, 400);
    ASSERT_EQ(complete_res->body, "Bad Request: 'output_url' must be a string.");
}

TEST_F(ApiTest, CompleteJobNonStringOutputUrl) {
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

    // Complete the job with non-string output_url
    nlohmann::json complete_payload;
    complete_payload["output_url"] = 123; // Non-string output_url
    auto complete_res = client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, complete_payload.dump(), "application/json");

    ASSERT_TRUE(complete_res != nullptr);
    ASSERT_EQ(complete_res->status, 400);
    ASSERT_EQ(complete_res->body, "Bad Request: 'output_url' must be a string.");
}

TEST_F(ApiTest, CompleteJobInvalidJson) {
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
    std::string invalid_json_payload = "{this is not json}";
    auto complete_res = client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, invalid_json_payload, "application/json");

    ASSERT_TRUE(complete_res != nullptr);
    ASSERT_EQ(complete_res->status, 400);
    ASSERT_TRUE(complete_res->body.rfind("Invalid JSON:", 0) == 0);
}

TEST_F(ApiTest, CompleteJobNoApiKey) {
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
    httplib::Headers no_api_key_headers = {
        {"Authorization", "some_token"}
    };
    auto complete_res = client->Post(("/jobs/" + job_id + "/complete").c_str(), no_api_key_headers, complete_payload.dump(), "application/json");

    ASSERT_TRUE(complete_res != nullptr);
    ASSERT_EQ(complete_res->status, 401);
}

TEST_F(ApiTest, FailJobAndRequeue) {
    // Submit a job with max_retries > 0
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    job_payload["max_retries"] = 1;
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
    ASSERT_EQ(fail_res->body, "Job " + job_id + " re-queued");

    // Verify job status
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        ASSERT_EQ(jobs_db[job_id]["status"], "pending");
        ASSERT_EQ(jobs_db[job_id]["retries"], 1);
        ASSERT_EQ(jobs_db[job_id]["error_message"], "Transcoding failed");
    }
}

TEST_F(ApiTest, FailJobInvalidJobId) {
    nlohmann::json fail_payload;
    fail_payload["error_message"] = "Transcoding failed";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/invalid_job_id/fail", headers, fail_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 404);
    ASSERT_EQ(res->body, "Job not found");
}

TEST_F(ApiTest, FailJobAndFailPermanently) {
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
    ASSERT_EQ(fail_res->body, "Job " + job_id + " failed permanently");

    // Verify job status
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        ASSERT_EQ(jobs_db[job_id]["status"], "failed_permanently");
        ASSERT_EQ(jobs_db[job_id]["error_message"], "Transcoding failed");
    }
}

TEST_F(ApiTest, FailJobMissingErrorMessage) {
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

    // Fail the job
    nlohmann::json fail_payload;
    auto fail_res = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_payload.dump(), "application/json");

    ASSERT_TRUE(fail_res != nullptr);
    ASSERT_EQ(fail_res->status, 400);
    ASSERT_EQ(fail_res->body, "Bad Request: 'error_message' is missing.");
}

TEST_F(ApiTest, FailJobInvalidJson) {
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

    // Fail the job
    std::string invalid_json_payload = "{this is not json}";
    auto fail_res = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, invalid_json_payload, "application/json");

    ASSERT_TRUE(fail_res != nullptr);
    ASSERT_EQ(fail_res->status, 400);
    ASSERT_TRUE(fail_res->body.rfind("Invalid JSON:", 0) == 0);
}

TEST_F(ApiTest, FailJobNoApiKey) {
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

    // Fail the job
    nlohmann::json fail_payload;
    fail_payload["error_message"] = "Transcoding failed";
    httplib::Headers no_api_key_headers = {
        {"Authorization", "some_token"}
    };
    auto fail_res = client->Post(("/jobs/" + job_id + "/fail").c_str(), no_api_key_headers, fail_payload.dump(), "application/json");

    ASSERT_TRUE(fail_res != nullptr);
    ASSERT_EQ(fail_res->status, 401);
}

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


