#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "dispatch_server_core.h"
#include "job_handlers.h"
#include "assignment_handler.h"
#include "nlohmann/json.hpp"
#include "repositories.h"
#include <thread>
#include <chrono>

using ::testing::_;
using ::testing::Return;

using namespace distconv::DispatchServer;

// Helper to create a dummy request
httplib::Request create_request(const std::string& body, const std::string& api_key = "test-key") {
    httplib::Request req;
    req.body = body;
    req.headers.emplace("X-API-Key", api_key);
    return req;
}

class JobManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear global state
        jobs_db.clear();
        engines_db.clear();
    }
    
    void TearDown() override {
        jobs_db.clear();
        engines_db.clear();
    }
};

TEST_F(JobManagementTest, PriorityAssignment) {
    // Setup: 1 high priority job, 1 normal priority job
    auto job_repo = std::make_shared<InMemoryJobRepository>();
    auto auth = std::make_shared<AuthMiddleware>("test-key");
    JobAssignmentHandler handler(auth, job_repo);
    
    // Create jobs directly in DB
    nlohmann::json job_normal;
    job_normal["job_id"] = "job_normal";
    job_normal["status"] = "pending";
    job_normal["priority"] = 0;
    job_normal["created_at"] = 1000;
    job_normal["assigned_engine"] = nullptr;
    jobs_db["job_normal"] = job_normal;
    job_repo->save_job("job_normal", job_normal);
    
    nlohmann::json job_high;
    job_high["job_id"] = "job_high";
    job_high["status"] = "pending";
    job_high["priority"] = 1;
    job_high["created_at"] = 2000; // Created later, but higher priority
    job_high["assigned_engine"] = nullptr;
    jobs_db["job_high"] = job_high;
    job_repo->save_job("job_high", job_high);
    
    // Register engine
    nlohmann::json engine;
    engine["engine_id"] = "engine1";
    engine["status"] = "idle";
    engine["benchmark_time"] = 10.0;
    engine["storage_capacity_gb"] = 1000.0;
    engines_db["engine1"] = engine;
    
    // Request assignment
    httplib::Request req;
    req.body = R"({"engine_id": "engine1"})";
    req.headers.emplace("X-API-Key", "test-key");
    httplib::Response res;
    
    handler.handle(req, res);
    
    EXPECT_EQ(res.status, 200);
    auto json_res = nlohmann::json::parse(res.body);
    EXPECT_EQ(json_res["job_id"], "job_high");
}

TEST_F(JobManagementTest, StorageRequirement) {
    auto job_repo = std::make_shared<InMemoryJobRepository>();
    auto auth = std::make_shared<AuthMiddleware>("test-key");
    JobAssignmentHandler handler(auth, job_repo);
    
    // Job requires 500MB (0.5GB)
    nlohmann::json job;
    job["job_id"] = "job1";
    job["status"] = "pending";
    job["job_size"] = 500.0; 
    job["assigned_engine"] = nullptr;
    jobs_db["job1"] = job;
    job_repo->save_job("job1", job);
    
    // Engine has only 0.1GB
    nlohmann::json engine;
    engine["engine_id"] = "engine1";
    engine["status"] = "idle";
    engine["storage_capacity_gb"] = 0.1;
    engines_db["engine1"] = engine;
    
    httplib::Request req;
    req.body = R"({"engine_id": "engine1"})";
    req.headers.emplace("X-API-Key", "test-key");
    httplib::Response res;
    
    handler.handle(req, res);
    
    // Should not assign
    EXPECT_EQ(res.status, 204);
}

TEST_F(JobManagementTest, ManualRetry) {
    auto auth = std::make_shared<AuthMiddleware>("test-key");
    JobRetryHandler handler(auth);
    
    jobs_db["job1"] = {
        {"job_id", "job1"},
        {"status", "failed"},
        {"retries", 3}
    };
    
    httplib::Request req;
    std::string path = "/jobs/job1/retry";
    std::regex re(R"(/jobs/(.+)/retry)");
    std::regex_match(path, req.matches, re);
    
    req.headers.emplace("X-API-Key", "test-key");
    httplib::Response res;
    
    handler.handle(req, res);
    
    EXPECT_EQ(res.status, 200);
    EXPECT_EQ(jobs_db["job1"]["status"], "pending");
    EXPECT_EQ(jobs_db["job1"]["retries"], 0);
}

TEST_F(JobManagementTest, CancelJob) {
    auto auth = std::make_shared<AuthMiddleware>("test-key");
    JobCancelHandler handler(auth);
    
    jobs_db["job1"] = {
        {"job_id", "job1"},
        {"status", "pending"},
        {"assigned_engine", nullptr}
    };
    
    httplib::Request req;
    std::string path = "/jobs/job1/cancel";
    std::regex re(R"(/jobs/(.+)/cancel)");
    std::regex_match(path, req.matches, re);
    
    req.headers.emplace("X-API-Key", "test-key");
    httplib::Response res;
    
    handler.handle(req, res);
    
    EXPECT_EQ(res.status, 200);
    EXPECT_EQ(jobs_db["job1"]["status"], "cancelled");
}
