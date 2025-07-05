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
        {"X-API-Key", api_key}
    };

    auto res = client->Post("/engines/heartbeat", headers, request_body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    // Verify the engine is in the database
    {
        std::lock_guard<std::mutex> lock(engines_mutex);
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
