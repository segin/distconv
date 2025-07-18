#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>

#include "test_common.h"

TEST_F(ApiTest, SchedulerAssignsJobToIdleEngine) {
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

    // 4. Check that the job was assigned
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["status"], "assigned");
    ASSERT_EQ(job_json["assigned_engine"], "engine-123");
}

TEST_F(ApiTest, SchedulerDoesNotAssignJobIfNoEngines) {
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

    // 2. Try to assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 204); // No Content, because no engines are registered
}

TEST_F(ApiTest, SchedulerDoesNotAssignJobIfAllEnginesAreBusy) {
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

    // 2. Register a busy engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "busy"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 3. Try to assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 204); // No Content, because all engines are busy
}

TEST_F(ApiTest, SchedulerAssignsLargeJobToStreamingEngine) {
    // 1. Create a large job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"job_size", 200.0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Register a non-streaming engine
    nlohmann::json engine_payload_1 = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 200.0}
    };
    auto res_heartbeat_1 = client->Post("/engines/heartbeat", admin_headers, engine_payload_1.dump(), "application/json");
    ASSERT_EQ(res_heartbeat_1->status, 200);

    // 3. Register a streaming-capable engine
    nlohmann::json engine_payload_2 = {
        {"engine_id", "engine-456"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0},
        {"streaming_support", true}
    };
    auto res_heartbeat_2 = client->Post("/engines/heartbeat", admin_headers, engine_payload_2.dump(), "application/json");
    ASSERT_EQ(res_heartbeat_2->status, 200);

    // 4. Assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "engine-456"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 5. Check that the job was assigned to the streaming engine
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["assigned_engine"], "engine-456");
}

TEST_F(ApiTest, SchedulerAssignsLargeJobToFastestNonStreamingEngine) {
    // 1. Create a large job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"job_size", 200.0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Register a slow, non-streaming engine
    nlohmann::json engine_payload_1 = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 200.0}
    };
    auto res_heartbeat_1 = client->Post("/engines/heartbeat", admin_headers, engine_payload_1.dump(), "application/json");
    ASSERT_EQ(res_heartbeat_1->status, 200);

    // 3. Register a fast, non-streaming engine
    nlohmann::json engine_payload_2 = {
        {"engine_id", "engine-456"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat_2 = client->Post("/engines/heartbeat", admin_headers, engine_payload_2.dump(), "application/json");
    ASSERT_EQ(res_heartbeat_2->status, 200);

    // 4. Assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "engine-456"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 5. Check that the job was assigned to the fastest engine
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["assigned_engine"], "engine-456");
}

TEST_F(ApiTest, SchedulerAssignsSmallJobToSlowestEngine) {
    // 1. Create a small job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"job_size", 10.0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Register a fast engine
    nlohmann::json engine_payload_1 = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat_1 = client->Post("/engines/heartbeat", admin_headers, engine_payload_1.dump(), "application/json");
    ASSERT_EQ(res_heartbeat_1->status, 200);

    // 3. Register a slow engine
    nlohmann::json engine_payload_2 = {
        {"engine_id", "engine-456"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 200.0}
    };
    auto res_heartbeat_2 = client->Post("/engines/heartbeat", admin_headers, engine_payload_2.dump(), "application/json");
    ASSERT_EQ(res_heartbeat_2->status, 200);

    // 4. Assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"} // It doesn't matter which engine we ask for, the scheduler should override
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 5. Check that the job was assigned to the slowest engine
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["assigned_engine"], "engine-456");
}

TEST_F(ApiTest, SchedulerAssignsMediumJobToFastestEngine) {
    // 1. Create a medium job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"job_size", 75.0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Register a slow engine
    nlohmann::json engine_payload_1 = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 200.0}
    };
    auto res_heartbeat_1 = client->Post("/engines/heartbeat", admin_headers, engine_payload_1.dump(), "application/json");
    ASSERT_EQ(res_heartbeat_1->status, 200);

    // 3. Register a fast engine
    nlohmann::json engine_payload_2 = {
        {"engine_id", "engine-456"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat_2 = client->Post("/engines/heartbeat", admin_headers, engine_payload_2.dump(), "application/json");
    ASSERT_EQ(res_heartbeat_2->status, 200);

    // 4. Assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"} // It doesn't matter which engine we ask for, the scheduler should override
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 5. Check that the job was assigned to the fastest engine
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["assigned_engine"], "engine-456");
}

TEST_F(ApiTest, SchedulerIgnoresEngineWithoutBenchmarkData) {
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

    // 2. Register an engine without benchmark data
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 3. Try to assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 204); // No Content, because the only engine has no benchmark data
}

TEST_F(ApiTest, SchedulerUpdatesEngineStatusToBusy) {
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

    // 4. Check the engine's status
    auto res_get_engine = client->Get("/engines/", admin_headers);
    ASSERT_EQ(res_get_engine->status, 200);
    nlohmann::json engines_json = nlohmann::json::parse(res_get_engine->body);
    ASSERT_TRUE(engines_json.is_array() && !engines_json.empty());
    ASSERT_EQ(engines_json[0]["status"], "busy");
}

TEST_F(ApiTest, SchedulerUpdatesJobStatusToAssigned) {
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

    // 4. Check the job's status
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["status"], "assigned");
}

TEST_F(ApiTest, SchedulerRecordsAssignedEngineId) {
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

    // 4. Check the job's assigned_engine
    auto res_get_job = client->Get(("/jobs/" + job_id).c_str(), admin_headers);
    ASSERT_EQ(res_get_job->status, 200);
    nlohmann::json job_json = nlohmann::json::parse(res_get_job->body);
    ASSERT_EQ(job_json["assigned_engine"], "engine-123");
}

TEST_F(ApiTest, CompletedJobNotReassigned) {
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
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db[job_id]["status"] = "completed";
    }

    // 5. Try to assign a job again
    auto res_assign_again = client->Post("/assign_job/", admin_headers, nlohmann::json({}).dump(), "application/json");
    ASSERT_EQ(res_assign_again->status, 204); // No Content, because the only job is completed
}

TEST_F(ApiTest, PermanentlyFailedJobIsNotReassigned) {
    // 1. Create a job with max_retries = 0 so it fails permanently on first failure
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", 0}
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

    // 3. Assign the job to the engine (this will make it assigned, then fail it)
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 4. Mark the job as failed (it will become permanently failed due to max_retries = 0)
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post("/jobs/" + job_id + "/fail", admin_headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);

    // 5. Try to assign a job again
    auto res_assign_again = client->Post("/assign_job/", admin_headers, nlohmann::json({}).dump(), "application/json");
    ASSERT_EQ(res_assign_again->status, 204); // No Content, because the only job is permanently failed
}


