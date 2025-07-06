#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>

#include "test_common.h"

TEST_F(ApiTest, SaveStateWritesJobsAndEngines) {
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

    // 3. Save the state
    save_state();

    // 4. Read the state file and verify its contents
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("jobs"));
    ASSERT_TRUE(state["jobs"].contains(job_id));
    ASSERT_EQ(state["jobs"][job_id]["source_url"], "http://example.com/video.mp4");

    ASSERT_TRUE(state.contains("engines"));
    ASSERT_TRUE(state["engines"].contains("engine-123"));
    ASSERT_EQ(state["engines"]["engine-123"]["status"], "idle");
}

TEST_F(ApiTest, LoadStateLoadsJobs) {
    // 1. Create a job and save the state to a temporary file
    std::string temp_storage_file = "temp_state.json";
    PERSISTENT_STORAGE_FILE = temp_storage_file;
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
    save_state();

    // 2. Clear the in-memory database
    clear_db();

    // 3. Load the state from the temporary file
    load_state();

    // 4. Verify that the job is loaded into the in-memory database
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        ASSERT_TRUE(jobs_db.contains(job_id));
        ASSERT_EQ(jobs_db[job_id]["source_url"], "http://example.com/video.mp4");
    }

    // 5. Clean up the temporary file
    std::remove(temp_storage_file.c_str());
}

TEST_F(ApiTest, LoadStateLoadsEngines) {
    // 1. Register an engine and save the state to a temporary file
    std::string temp_storage_file = "temp_state.json";
    PERSISTENT_STORAGE_FILE = temp_storage_file;
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
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    save_state();

    // 2. Clear the in-memory database
    clear_db();

    // 3. Load the state from the temporary file
    load_state();

    // 4. Verify that the engine is loaded into the in-memory database
    {
        std::lock_guard<std::mutex> lock(engines_mutex);
        ASSERT_TRUE(engines_db.contains("engine-123"));
        ASSERT_EQ(engines_db["engine-123"]["status"], "idle");
    }

    // 5. Clean up the temporary file
    std::remove(temp_storage_file.c_str());
}
