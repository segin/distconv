#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "job_handlers.h"
#include "dispatch_server_core.h"
#include "request_handlers.h"
#include <regex>


// Helper to create a request with path match
static httplib::Request make_request(const std::string& job_id) {
    httplib::Request req;
    req.path = "/jobs/" + job_id;
    // Simulate regex match results - httplib populates this when using regex routes
    // We need to manually set it for testing
    std::smatch m;
    std::string path = req.path;
    std::regex pattern(R"(/jobs/(.+))");
    if (std::regex_match(path, m, pattern)) {
        req.matches = m;
    }
    return req;
}

TEST(JobHandlersTest, JobStatusHandlerFound) {
    // No auth required
    auto auth = std::make_shared<AuthMiddleware>("");
    JobStatusHandler handler(auth);
    // Insert a job into the global DB
    std::string job_id = "test-job-id-1234";
    nlohmann::json job;
    job["job_id"] = job_id;
    job["status"] = "pending";
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db[job_id] = job;
    }
    httplib::Response res;
    auto req = make_request(job_id);
    handler.handle(req, res);
    EXPECT_EQ(res.status, 200);
    nlohmann::json parsed = nlohmann::json::parse(res.body);
    EXPECT_EQ(parsed["job_id"], job_id);
    EXPECT_EQ(parsed["status"], "pending");
}

TEST(JobHandlersTest, JobStatusHandlerNotFound) {
    auto auth = std::make_shared<AuthMiddleware>("");
    JobStatusHandler handler(auth);
    httplib::Response res;
    auto req = make_request("nonexistent-id");
    handler.handle(req, res);
    EXPECT_EQ(res.status, 404);
}

TEST(JobHandlersTest, JobListHandlerEmpty) {
    // Ensure DB is empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db.clear();
    }
    auto auth = std::make_shared<AuthMiddleware>("");
    JobListHandler handler(auth);
    httplib::Response res;
    httplib::Request req; // no matches needed for list
    handler.handle(req, res);
    EXPECT_EQ(res.status, 200);
    nlohmann::json parsed = nlohmann::json::parse(res.body);
    EXPECT_TRUE(parsed.is_array());
    EXPECT_TRUE(parsed.empty());
}

TEST(JobHandlersTest, JobListHandlerWithJobs) {
    // Populate DB with two jobs
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db.clear();
        for (int i = 0; i < 2; ++i) {
            std::string id = "job-" + std::to_string(i);
            nlohmann::json job;
            job["job_id"] = id;
            job["status"] = "pending";
            jobs_db[id] = job;
        }
    }
    auto auth = std::make_shared<AuthMiddleware>("");
    JobListHandler handler(auth);
    httplib::Response res;
    httplib::Request req;
    handler.handle(req, res);
    EXPECT_EQ(res.status, 200);
    nlohmann::json parsed = nlohmann::json::parse(res.body);
    EXPECT_TRUE(parsed.is_array());
    EXPECT_EQ(parsed.size(), 2);
}
