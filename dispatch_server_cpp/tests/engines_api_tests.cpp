#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream> // Required for std::ofstream
#include <cstdio> // Required for std::remove
#include <thread>
#include <chrono>

#include "test_common.h" // For ApiTest fixture and clear_db()

TEST_F(ApiTest, RegisterEngineValid) {
    nlohmann::json request_body;
    request_body["engine_id"] = "test_engine";
    request_body["engine_type"] = "transcoder";
    request_body["supported_codecs"] = {"h264", "vp9"};

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    auto res = client->Post("/engines/heartbeat", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    // Verify the engine is in the database
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(engines_db.contains("test_engine"));
    }
}

TEST_F(ApiTest, RegisterEngineDuplicate) {
    // Register an engine
    nlohmann::json request_body;
    request_body["engine_id"] = "test_engine";
    request_body["engine_type"] = "transcoder";
    request_body["supported_codecs"] = {"h264", "vp9"};

    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };

    auto res1 = client->Post("/engines/heartbeat", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res1 != nullptr);
    ASSERT_EQ(res1->status, 200);

    // Try to register the same engine again
    auto res2 = client->Post("/engines/heartbeat", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res2 != nullptr);
    ASSERT_EQ(res2->status, 200); 
}

TEST_F(ApiTest, RegisterEngineNoApiKey) {
    // Create a dummy engine for testing
    nlohmann::json request_body;
    request_body["engine_id"] = "test_engine";
    request_body["engine_type"] = "transcoder";
    request_body["supported_codecs"] = {"h264", "vp9"};

    auto res = client->Post("/engines/heartbeat", request_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401); // Unauthorized
}

TEST_F(ApiTest, RegisterEngineIncorrectApiKey) {
    // Create a dummy engine for testing
    nlohmann::json request_body;
    request_body["engine_id"] = "test_engine";
    request_body["engine_type"] = "transcoder";
    request_body["supported_codecs"] = {"h264", "vp9"};

    httplib::Headers headers = {
        {"X-API-Key", "wrong_api_key"}
    };

    auto res = client->Post("/engines/heartbeat", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401); // Unauthorized
}

TEST_F(ApiTest, ListEnginesEmpty) {
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Get("/engines/", headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_TRUE(response_json.empty());
}

TEST_F(ApiTest, ListEnginesWithEngines) {
    // Create some dummy engines
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    for (int i = 0; i < 3; ++i) {
        nlohmann::json request_body;
        request_body["engine_id"] = "test_engine_" + std::to_string(i);
        request_body["engine_type"] = "transcoder";
        request_body["supported_codecs"] = {"h264", "vp9"};
        auto res_post = client->Post("/engines/heartbeat", headers, request_body.dump(), "application/json");
        ASSERT_TRUE(res_post != nullptr);
        ASSERT_EQ(res_post->status, 200);
    }

    auto res = client->Get("/engines/", headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 3);
}


TEST_F(ApiTest, ListEnginesWithOneEngine) {
    // Create a dummy engine
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    nlohmann::json request_body;
    request_body["engine_id"] = "test_engine";
    request_body["engine_type"] = "transcoder";
    request_body["supported_codecs"] = {"h264", "vp9"};
    auto res_post = client->Post("/engines/heartbeat", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res_post != nullptr);
    ASSERT_EQ(res_post->status, 200);

    auto res = client->Get("/engines/", headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 1);
}

TEST_F(ApiTest, EngineStatusIsIdleAfterHeartbeat) {
    // 1. Send a heartbeat
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 2. Get the engine status
    auto res_get = client->Get("/engines/", admin_headers);
    ASSERT_EQ(res_get->status, 200);

    // 3. Assert that the engine status is "idle"
    nlohmann::json response_json = nlohmann::json::parse(res_get->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 1);
    ASSERT_EQ(response_json[0]["status"], "idle");
}


TEST_F(ApiTest, EngineStatusIsBusyAfterJobAssign) {
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
        {"benchmark_time", 100}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 3. Assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 4. Get the engine status
    auto res_get = client->Get("/engines/", admin_headers);
    ASSERT_EQ(res_get->status, 200);

    // 5. Assert that the engine status is "busy"
    nlohmann::json response_json = nlohmann::json::parse(res_get->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 1);
    ASSERT_EQ(response_json[0]["status"], "busy");
}




TEST_F(ApiTest, BusyEngineIsNotAssignedAnotherJob) {
    // 1. Register an engine
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

    // 2. Create two jobs
    nlohmann::json job_payload_1 = {
        {"source_url", "http://example.com/video1.mp4"},
        {"target_codec", "h264"}
    };
    auto res_submit_1 = client->Post("/jobs/", admin_headers, job_payload_1.dump(), "application/json");
    ASSERT_EQ(res_submit_1->status, 200);
    std::string job_id_1 = nlohmann::json::parse(res_submit_1->body)["job_id"];

    nlohmann::json job_payload_2 = {
        {"source_url", "http://example.com/video2.mp4"},
        {"target_codec", "vp9"}
    };
    auto res_submit_2 = client->Post("/jobs/", admin_headers, job_payload_2.dump(), "application/json");
    ASSERT_EQ(res_submit_2->status, 200);
    std::string job_id_2 = nlohmann::json::parse(res_submit_2->body)["job_id"];

    // 3. Assign the first job to the engine
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign_1 = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign_1->status, 200);

    // 4. Try to assign the second job
    auto res_assign_2 = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign_2->status, 204); // No Content, because the only engine is busy
}

