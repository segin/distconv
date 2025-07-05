#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h" // For ApiTest fixture and clear_db()

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
        {"X-API-Key", api_key}
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
        {"X-API-Key", api_key}
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
