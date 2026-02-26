#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "../repositories.h"
#include <cstdio>
#include <thread>
#include <chrono>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;
using namespace distconv::DispatchServer;

class ModernAssignmentTest : public ::testing::Test {
protected:
    std::string job_db_path = "test_jobs.db";
    std::string engine_db_path = "test_engines.db";
    std::shared_ptr<SqliteJobRepository> job_repo;
    std::shared_ptr<SqliteEngineRepository> engine_repo;
    std::unique_ptr<DispatchServer> server;
    std::unique_ptr<httplib::Client> client;
    int port = 8089;

    void SetUp() override {
        if (fs::exists(job_db_path)) fs::remove(job_db_path);
        if (fs::exists(engine_db_path)) fs::remove(engine_db_path);

        job_repo = std::make_shared<SqliteJobRepository>(job_db_path);
        engine_repo = std::make_shared<SqliteEngineRepository>(engine_db_path);

        server = std::make_unique<DispatchServer>(job_repo, engine_repo, "test_key");
        server->start(port, false);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        client = std::make_unique<httplib::Client>("localhost", port);
    }

    void TearDown() override {
        if (server) server->stop();
        server.reset();
        client.reset();
        if (fs::exists(job_db_path)) fs::remove(job_db_path);
        if (fs::exists(engine_db_path)) fs::remove(engine_db_path);
    }
};

TEST_F(ModernAssignmentTest, AssignsPendingJobUsingOptimizedQuery) {
    // 1. Add a pending job to repo
    nlohmann::json job;
    job["job_id"] = "job-1";
    job["status"] = "pending";
    job["priority"] = 10;
    job["created_at"] = 1000;
    job_repo->save_job("job-1", job);

    // 2. Add an engine to repo
    nlohmann::json engine;
    engine["engine_id"] = "engine-1";
    engine["status"] = "idle";
    engine_repo->save_engine("engine-1", engine);

    // 3. Request assignment
    nlohmann::json assign_req;
    assign_req["engine_id"] = "engine-1";

    httplib::Headers headers = {
        {"X-API-Key", "test_key"}
    };

    auto res = client->Post("/assign_job/", headers, assign_req.dump(), "application/json");

    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);

    nlohmann::json assigned_job = nlohmann::json::parse(res->body);
    EXPECT_EQ(assigned_job["job_id"], "job-1");
    EXPECT_EQ(assigned_job["status"], "assigned");
    EXPECT_EQ(assigned_job["assigned_engine"], "engine-1");

    // 4. Verify job updated in repo
    nlohmann::json updated_job = job_repo->get_job("job-1");
    EXPECT_EQ(updated_job["status"], "assigned");
    EXPECT_EQ(updated_job["assigned_engine"], "engine-1");

    // 5. Verify engine updated in repo
    nlohmann::json updated_engine = engine_repo->get_engine("engine-1");
    EXPECT_EQ(updated_engine["status"], "busy");
}

TEST_F(ModernAssignmentTest, ReturnsNoContentWhenNoPendingJobs) {
    // 1. Repo is empty of pending jobs (or has completed jobs)
    nlohmann::json job;
    job["job_id"] = "job-2";
    job["status"] = "completed";
    job_repo->save_job("job-2", job);

    // 2. Request assignment
    nlohmann::json assign_req;
    assign_req["engine_id"] = "engine-1";

    httplib::Headers headers = {
        {"X-API-Key", "test_key"}
    };

    auto res = client->Post("/assign_job/", headers, assign_req.dump(), "application/json");

    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 204);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
