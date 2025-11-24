#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "engine_handlers.h"
#include "dispatch_server_core.h"
#include "request_handlers.h"
#include <regex>

TEST(EngineHandlersTest, EngineListHandlerEmpty) {
    // Ensure DB is empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        engines_db.clear();
    }
    auto auth = std::make_shared<AuthMiddleware>("");
    EngineListHandler handler(auth);
    httplib::Response res;
    httplib::Request req;
    handler.handle(req, res);
    EXPECT_EQ(res.status, 200);
    nlohmann::json parsed = nlohmann::json::parse(res.body);
    EXPECT_TRUE(parsed.is_array());
    EXPECT_TRUE(parsed.empty());
}

TEST(EngineHandlersTest, EngineListHandlerWithEngines) {
    // Populate DB
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        engines_db.clear();
        nlohmann::json engine;
        engine["engine_id"] = "engine-1";
        engine["status"] = "idle";
        engines_db["engine-1"] = engine;
    }
    auto auth = std::make_shared<AuthMiddleware>("");
    EngineListHandler handler(auth);
    httplib::Response res;
    httplib::Request req;
    handler.handle(req, res);
    EXPECT_EQ(res.status, 200);
    nlohmann::json parsed = nlohmann::json::parse(res.body);
    EXPECT_TRUE(parsed.is_array());
    EXPECT_EQ(parsed.size(), 1);
    EXPECT_EQ(parsed[0]["engine_id"], "engine-1");
}

TEST(EngineHandlersTest, EngineHeartbeatValid) {
    auto auth = std::make_shared<AuthMiddleware>("");
    EngineHeartbeatHandler handler(auth);
    httplib::Response res;
    httplib::Request req;
    
    nlohmann::json body;
    body["engine_id"] = "engine-new";
    body["storage_capacity_gb"] = 100.0;
    body["can_stream"] = true;
    req.body = body.dump();
    
    handler.handle(req, res);
    EXPECT_EQ(res.status, 200);
    
    // Verify DB update
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        EXPECT_TRUE(engines_db.contains("engine-new"));
        EXPECT_EQ(engines_db["engine-new"]["storage_capacity_gb"], 100.0);
    }
}

TEST(EngineHandlersTest, EngineHeartbeatInvalidStorage) {
    auto auth = std::make_shared<AuthMiddleware>("");
    EngineHeartbeatHandler handler(auth);
    httplib::Response res;
    httplib::Request req;
    
    nlohmann::json body;
    body["engine_id"] = "engine-bad";
    body["storage_capacity_gb"] = -50.0; // Invalid
    req.body = body.dump();
    
    handler.handle(req, res);
    EXPECT_EQ(res.status, 400);
    nlohmann::json error = nlohmann::json::parse(res.body);
    EXPECT_EQ(error["error"]["code"], "BAD_REQUEST");
}

TEST(EngineHandlersTest, EngineBenchmarkValid) {
    // Setup engine first
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        engines_db["engine-bench"] = {{"engine_id", "engine-bench"}};
    }
    
    auto auth = std::make_shared<AuthMiddleware>("");
    EngineBenchmarkHandler handler(auth);
    httplib::Response res;
    httplib::Request req;
    
    nlohmann::json body;
    body["engine_id"] = "engine-bench";
    body["benchmark_time"] = 12.5;
    req.body = body.dump();
    
    handler.handle(req, res);
    EXPECT_EQ(res.status, 200);
    
    // Verify DB update
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        EXPECT_EQ(engines_db["engine-bench"]["benchmark_time"], 12.5);
    }
}

TEST(EngineHandlersTest, EngineBenchmarkNotFound) {
    auto auth = std::make_shared<AuthMiddleware>("");
    EngineBenchmarkHandler handler(auth);
    httplib::Response res;
    httplib::Request req;
    
    nlohmann::json body;
    body["engine_id"] = "engine-missing";
    body["benchmark_time"] = 12.5;
    req.body = body.dump();
    
    handler.handle(req, res);
    EXPECT_EQ(res.status, 404);
}
