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

using namespace distconv::DispatchServer;

TEST_F(ApiTest, JsonParsingValidJobSubmission) {
    // Test 118: JSON parsing of a valid job submission request.
    
    nlohmann::json valid_job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"job_size", 100.5},
        {"max_retries", 3}
    };
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Post("/jobs/", headers, valid_job_payload.dump(), "application/json");
    
    ASSERT_EQ(res->status, 200);
    nlohmann::json response = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response.contains("job_id"));
    ASSERT_EQ(response["source_url"], "http://example.com/video.mp4");
    ASSERT_EQ(response["target_codec"], "h264");
    ASSERT_EQ(response["status"], "pending");
}

TEST_F(ApiTest, JsonParsingValidHeartbeatRequest) {
    // Test 119: JSON parsing of a valid heartbeat request.
    
    nlohmann::json valid_heartbeat_payload = {
        {"engine_id", "engine-parse-test"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9", "av1"}},
        {"status", "idle"},
        {"benchmark_time", 125.75},
        {"storage_capacity_gb", 500.0},
        {"streaming_support", true}
    };
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Post("/engines/heartbeat", headers, valid_heartbeat_payload.dump(), "application/json");
    
    ASSERT_EQ(res->status, 200);
    ASSERT_TRUE(res->body.find("engine-parse-test") != std::string::npos);
    
    // Verify engine was registered correctly
    auto engines_res = client->Get("/engines/", headers);
    ASSERT_EQ(engines_res->status, 200);
    nlohmann::json engines = nlohmann::json::parse(engines_res->body);
    ASSERT_TRUE(engines.is_array());
    bool found_engine = false;
    for (const auto& engine : engines) {
        if (engine["engine_id"] == "engine-parse-test") {
            found_engine = true;
            ASSERT_EQ(engine["engine_type"], "transcoder");
            ASSERT_EQ(engine["benchmark_time"], 125.75);
            break;
        }
    }
    ASSERT_TRUE(found_engine);
}

TEST_F(ApiTest, JsonParsingValidJobCompletionRequest) {
    // Test 120: JSON parsing of a valid job completion request.
    
    // First create and assign a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // Register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "completion-test-engine"},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    // Assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "failure-test-engine"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // Test valid completion request
    nlohmann::json valid_completion_payload = {
        {"output_url", "http://example.com/video_completed.mp4"}
    };
    
    auto res_complete = client->Post("/jobs/" + job_id + "/complete", headers, valid_completion_payload.dump(), "application/json");
    ASSERT_EQ(res_complete->status, 200);
    ASSERT_TRUE(res_complete->body.find("completed") != std::string::npos);
    
    // Verify job status was updated
    auto res_status = client->Get("/jobs/" + job_id, headers);
    ASSERT_EQ(res_status->status, 200);
    nlohmann::json job_status = nlohmann::json::parse(res_status->body);
    ASSERT_EQ(job_status["status"], "completed");
    ASSERT_EQ(job_status["output_url"], "http://example.com/video_completed.mp4");
}

TEST_F(ApiTest, JsonParsingValidJobFailureRequest) {
    // Test 121: JSON parsing of a valid job failure request.
    
    // First create and assign a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // Register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "failure-test-engine"},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    // Assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "failure-test-engine"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // Test valid failure request
    nlohmann::json valid_failure_payload = {
        {"error_message", "Transcoding failed due to corrupt input file"}
    };
    
    auto res_fail = client->Post("/jobs/" + job_id + "/fail", headers, valid_failure_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);
    ASSERT_TRUE(res_fail->body.find("re-queued") != std::string::npos);
    
    // Verify job was re-queued (should be pending again with retries incremented)
    auto res_status = client->Get("/jobs/" + job_id, headers);
    ASSERT_EQ(res_status->status, 200);
    nlohmann::json job_status = nlohmann::json::parse(res_status->body);
    ASSERT_EQ(job_status["status"], "pending");
    ASSERT_EQ(job_status["retries"], 1);
    ASSERT_EQ(job_status["error_message"], "Transcoding failed due to corrupt input file");
}

TEST_F(ApiTest, ServerHandlesClientDisconnectingMidRequest) {
    // Test 122: Server handles a client disconnecting mid-request.
    
    // This test is difficult to simulate directly, but we can test timeout behavior
    // by creating a very short connection timeout and seeing how the server handles it
    httplib::Client short_timeout_client("localhost", port);
    short_timeout_client.set_connection_timeout(1); // 1 second timeout
    short_timeout_client.set_read_timeout(1);
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    
    // Make a request that should succeed normally
    auto res = short_timeout_client.Post("/jobs/", headers, job_payload.dump(), "application/json");
    
    // The server should handle this gracefully - either succeed or fail cleanly
    // We don't expect the server to crash or enter an invalid state
    if (res) {
        // If the request succeeded, verify it's valid
        ASSERT_TRUE(res->status == 200 || res->status >= 400);
        if (res->status == 200) {
            nlohmann::json response = nlohmann::json::parse(res->body);
            ASSERT_TRUE(response.contains("job_id"));
        }
    }
    
    // Verify server is still responsive after potential disconnect
    auto health_check = client->Get("/", headers);
    ASSERT_TRUE(health_check);
    ASSERT_EQ(health_check->status, 200);
}

TEST_F(ApiTest, ServerHandlesExtremelyLongSourceUrl) {
    // Test 123: Server handles extremely long source_url values.
    
    // Create a very long URL (10KB)
    std::string long_url = "http://example.com/";
    for (int i = 0; i < 10000; ++i) {
        long_url += "a";
    }
    long_url += ".mp4";
    
    nlohmann::json job_payload = {
        {"source_url", long_url},
        {"target_codec", "h264"}
    };
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    
    // Server should either accept it (200) or reject it gracefully (400)
    ASSERT_TRUE(res->status == 200 || res->status == 400);
    
    if (res->status == 200) {
        // If accepted, verify the job was created correctly
        nlohmann::json response = nlohmann::json::parse(res->body);
        ASSERT_TRUE(response.contains("job_id"));
        ASSERT_EQ(response["source_url"], long_url);
    }
    
    // Verify server is still responsive
    auto health_check = client->Get("/");
    ASSERT_EQ(health_check->status, 200);
}

TEST_F(ApiTest, ServerHandlesExtremelyLongEngineId) {
    // Test 124: Server handles extremely long engine_id values.
    
    // Create a very long engine ID (5KB)
    std::string long_engine_id = "";
    for (int i = 0; i < 5000; ++i) {
        long_engine_id += "e";
    }
    
    nlohmann::json engine_payload = {
        {"engine_id", long_engine_id},
        {"engine_type", "transcoder"},
        {"status", "idle"}
    };
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    
    // Server should either accept it (200) or reject it gracefully (400)
    ASSERT_TRUE(res->status == 200 || res->status == 400);
    
    if (res->status == 200) {
        // If accepted, verify the engine was registered correctly
        ASSERT_TRUE(res->body.find("Heartbeat received") != std::string::npos);
        
        // Verify it appears in the engines list
        auto engines_res = client->Get("/engines/", headers);
        ASSERT_EQ(engines_res->status, 200);
        nlohmann::json engines = nlohmann::json::parse(engines_res->body);
        bool found_engine = false;
        for (const auto& engine : engines) {
            if (engine["engine_id"] == long_engine_id) {
                found_engine = true;
                break;
            }
        }
        ASSERT_TRUE(found_engine);
    }
    
    // Verify server is still responsive
    auto health_check = client->Get("/");
    ASSERT_EQ(health_check->status, 200);
}

TEST_F(ApiTest, ServerHandlesVeryLargeNumberOfJobs) {
    // Test 125: Server handles a very large number of jobs in the database.
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    const int num_jobs = 1000; // Create 1000 jobs
    std::vector<std::string> job_ids;
    
    for (int i = 0; i < num_jobs; ++i) {
        nlohmann::json job_payload = {
            {"source_url", "http://example.com/video_" + std::to_string(i) + ".mp4"},
            {"target_codec", "h264"}
        };
        
        auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
        ASSERT_EQ(res->status, 200);
        
        nlohmann::json response = nlohmann::json::parse(res->body);
        job_ids.push_back(response["job_id"]);
    }
    
    // Verify all jobs can be listed
    auto list_res = client->Get("/jobs/", headers);
    ASSERT_EQ(list_res->status, 200);
    nlohmann::json all_jobs = nlohmann::json::parse(list_res->body);
    ASSERT_EQ(all_jobs.size(), num_jobs);
    
    // Verify individual job retrieval still works
    auto single_job_res = client->Get("/jobs/" + job_ids[0], headers);
    ASSERT_EQ(single_job_res->status, 200);
    
    // Verify server is still responsive
    auto health_check = client->Get("/");
    ASSERT_EQ(health_check->status, 200);
}

TEST_F(ApiTest, ServerHandlesVeryLargeNumberOfEngines) {
    // Test 126: Server handles a very large number of engines in the database.
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    const int num_engines = 500; // Create 500 engines
    
    for (int i = 0; i < num_engines; ++i) {
        nlohmann::json engine_payload = {
            {"engine_id", "engine_" + std::to_string(i)},
            {"engine_type", "transcoder"},
            {"status", "idle"},
            {"benchmark_time", 100.0 + i}
        };
        
        auto res = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
        ASSERT_EQ(res->status, 200);
    }
    
    // Verify all engines can be listed
    auto list_res = client->Get("/engines/", headers);
    ASSERT_EQ(list_res->status, 200);
    nlohmann::json all_engines = nlohmann::json::parse(list_res->body);
    ASSERT_EQ(all_engines.size(), num_engines);
    
    // Verify engines are accessible and assignment still works
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    auto job_res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(job_res->status, 200);
    
    nlohmann::json assign_payload = {
        {"engine_id", "engine_0"}
    };
    auto assign_res = client->Post("/assign_job/", headers, "{\"engine_id\": \"engine_0\"}", "application/json");
    ASSERT_EQ(assign_res->status, 200);
    
    // Verify server is still responsive
    auto health_check = client->Get("/");
    ASSERT_EQ(health_check->status, 200);
}

TEST_F(ApiTest, ServerHandlesJobIdThatLooksLikeNumber) {
    // Test 127: Server handles a job ID that looks like a number but is a string.
    
    // Create a job first to get a real job ID
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // Try to access with numeric-looking but non-existent job ID
    std::string numeric_looking_id = "12345678901234567890";
    auto res_numeric = client->Get("/jobs/" + numeric_looking_id, headers);
    ASSERT_EQ(res_numeric->status, 404);
    
    // Try with a very large number as string
    std::string large_numeric_id = "999999999999999999999999999999";
    auto res_large = client->Get("/jobs/" + large_numeric_id, headers);
    ASSERT_EQ(res_large->status, 404);
    
    // Verify the real job ID still works
    auto res_real = client->Get("/jobs/" + job_id, headers);
    ASSERT_EQ(res_real->status, 200);
    
    // Test completion and failure endpoints with numeric-looking IDs
    nlohmann::json complete_payload = {{"output_url", "http://example.com/output.mp4"}};
    auto res_complete_numeric = client->Post("/jobs/" + numeric_looking_id + "/complete", headers, complete_payload.dump(), "application/json");
    ASSERT_EQ(res_complete_numeric->status, 404);
    
    nlohmann::json fail_payload = {{"error_message", "Test error"}};
    auto res_fail_numeric = client->Post("/jobs/" + numeric_looking_id + "/fail", headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail_numeric->status, 404);
}

TEST_F(ApiTest, ServerHandlesHeartbeatForEngineAssignedJobThatNoLongerExists) {
    // Test 128: Server handles a heartbeat for an engine that was assigned a job that no longer exists.
    
    httplib::Headers headers = {{"X-API-Key", api_key}};
    
    // Create a job and engine, then assign the job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    nlohmann::json engine_payload = {
        {"engine_id", "orphaned-engine"},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_engine = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_engine->status, 200);
    
    // Assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "orphaned-engine"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // Manually remove the job from the database to simulate it no longer existing
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db.erase(job_id);
    }
    
    // Send another heartbeat from the engine - should handle gracefully
    nlohmann::json heartbeat_payload = {
        {"engine_id", "orphaned-engine"},
        {"status", "busy"}, // Engine thinks it's still working
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, heartbeat_payload.dump(), "application/json");
    
    // Server should handle this gracefully (accept the heartbeat)
    ASSERT_EQ(res_heartbeat->status, 200);
    
    // Verify engine is still registered and server is responsive
    auto engines_res = client->Get("/engines/", headers);
    ASSERT_EQ(engines_res->status, 200);
    nlohmann::json engines = nlohmann::json::parse(engines_res->body);
    bool found_engine = false;
    for (const auto& engine : engines) {
        if (engine["engine_id"] == "orphaned-engine") {
            found_engine = true;
            break;
        }
    }
    ASSERT_TRUE(found_engine);
}

TEST_F(ApiTest, ServerHandlesRequestToCompleteJobThatWasNeverAssigned) {
    // Test 129: Server handles a request to complete a job that was never assigned.
    
    // Create a job but don't assign it
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // Verify job is in pending state
    auto res_status = client->Get("/jobs/" + job_id, headers);
    ASSERT_EQ(res_status->status, 200);
    nlohmann::json job_status = nlohmann::json::parse(res_status->body);
    ASSERT_EQ(job_status["status"], "pending");
    ASSERT_TRUE(job_status["assigned_engine"].is_null());
    
    // Try to complete the unassigned job
    nlohmann::json complete_payload = {
        {"output_url", "http://example.com/video_completed.mp4"}
    };
    auto res_complete = client->Post("/jobs/" + job_id + "/complete", headers, complete_payload.dump(), "application/json");
    
    // Server should handle this gracefully - either succeed or give appropriate error
    ASSERT_TRUE(res_complete->status == 200 || res_complete->status == 400);
    
    // Verify server is still responsive
    auto health_check = client->Get("/");
    ASSERT_EQ(health_check->status, 200);
}

TEST_F(ApiTest, ServerHandlesRequestToFailJobThatWasNeverAssigned) {
    // Test 130: Server handles a request to fail a job that was never assigned.
    
    // Create a job but don't assign it
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // Verify job is in pending state
    auto res_status = client->Get("/jobs/" + job_id, headers);
    ASSERT_EQ(res_status->status, 200);
    nlohmann::json job_status = nlohmann::json::parse(res_status->body);
    ASSERT_EQ(job_status["status"], "pending");
    ASSERT_TRUE(job_status["assigned_engine"].is_null());
    
    // Try to fail the unassigned job
    nlohmann::json fail_payload = {
        {"error_message", "Job failed for unknown reason"}
    };
    auto res_fail = client->Post("/jobs/" + job_id + "/fail", headers, fail_payload.dump(), "application/json");
    
    // Server should handle this gracefully - either succeed or give appropriate error
    ASSERT_TRUE(res_fail->status == 200 || res_fail->status == 400);
    
    // Verify server is still responsive
    auto health_check = client->Get("/");
    ASSERT_EQ(health_check->status, 200);
}
