#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h"
#include <memory>
#include <thread>

using namespace distconv::DispatchServer;

class ModernApiTest : public ::testing::Test {
protected:
    DispatchServer *server;
    httplib::Client *client;
    int port;
    std::string api_key;
    std::shared_ptr<IJobRepository> job_repo;
    std::shared_ptr<IEngineRepository> engine_repo;

    void SetUp() override {
        port = find_available_port();
        api_key = "test_api_key";

        // Use InMemory repositories for speed and simplicity in unit tests
        job_repo = std::make_shared<InMemoryJobRepository>();
        engine_repo = std::make_shared<InMemoryEngineRepository>();

        // Use the dependency injection constructor
        server = new DispatchServer(job_repo, engine_repo, api_key);
        server->start(port, false);

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give it a moment to start

        client = new httplib::Client("localhost", port);
        client->set_connection_timeout(5);

        // Wait for server
        int retries = 20;
        while (retries-- > 0) {
            auto res = client->Get("/");
            if (res && res->status == 200) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void TearDown() override {
        server->stop();
        delete server;
        delete client;
    }
};

TEST_F(ModernApiTest, AssignJob_OptimizedPath) {
    // 1. Create a job directly in the repo
    nlohmann::json job;
    job["job_id"] = "job_1";
    job["status"] = "pending";
    job["priority"] = 10;
    job["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    job["source_url"] = "http://example.com/video.mp4";
    job["target_codec"] = "h264";

    job_repo->save_job("job_1", job);

    // 2. Register an engine directly in the repo
    nlohmann::json engine;
    engine["engine_id"] = "engine_1";
    engine["status"] = "idle";
    engine["supported_codecs"] = {"h264"};

    engine_repo->save_engine("engine_1", engine);

    // 3. Request job assignment via API (hitting the optimized path)
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    nlohmann::json assign_payload = {
        {"engine_id", "engine_1"}
    };

    auto res = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");

    ASSERT_EQ(res->status, 200);
    nlohmann::json body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["job_id"], "job_1");
    EXPECT_EQ(body["status"], "assigned");
    EXPECT_EQ(body["assigned_engine"], "engine_1");

    // 4. Verify updates in repo
    auto updated_job = job_repo->get_job("job_1");
    EXPECT_EQ(updated_job["status"], "assigned");
    EXPECT_EQ(updated_job["assigned_engine"], "engine_1");

    auto updated_engine = engine_repo->get_engine("engine_1");
    EXPECT_EQ(updated_engine["status"], "busy");
}

TEST_F(ModernApiTest, AssignJob_NoPendingJobs) {
    // 1. No jobs in repo

    // 2. Register engine
    nlohmann::json engine;
    engine["engine_id"] = "engine_1";
    engine["status"] = "idle";

    engine_repo->save_engine("engine_1", engine);

    // 3. Request assignment
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    nlohmann::json assign_payload = {
        {"engine_id", "engine_1"}
    };

    auto res = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");

    // Expect 204 No Content
    ASSERT_EQ(res->status, 204);
}
