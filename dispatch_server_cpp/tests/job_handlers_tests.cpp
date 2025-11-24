#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "job_handlers.h"
#include "dispatch_server_core.h"
#include "request_handlers.h"
#include "job_action_handlers.h"
#include "assignment_handler.h"
#include "assignment_handler.h"
#include "repositories.h"
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

TEST(JobHandlersTest, JobCompletionValid) {
    // Setup job
    std::string job_id = "job-complete-1";
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        nlohmann::json job;
        job["job_id"] = job_id;
        job["status"] = "pending";
        jobs_db[job_id] = job;
    }
    
    auto auth = std::make_shared<AuthMiddleware>("");
    JobCompletionHandler handler(auth);
    httplib::Response res;
    
    // Create request with regex match
    httplib::Request req;
    req.path = "/jobs/" + job_id + "/complete";
    req.matches = std::smatch(); // Mock matches
    // We need to manually construct matches since we can't easily mock std::smatch with specific values
    // In a real unit test we might need a better way, but for now we rely on the handler parsing req.matches[1]
    // Actually, std::smatch is hard to mock. We might need to refactor handler to take job_id or use a different approach.
    // However, for this test environment, we can rely on integration tests for path parsing, 
    // OR we can try to populate matches if possible.
    // Since we can't easily populate std::smatch, let's skip the unit test for handlers that rely heavily on regex matches
    // and rely on integration tests for those specific parts, OR we can refactor the handler to be more testable.
    // BUT, wait! I can use std::regex_match to populate it like in the helper function!
}

// Helper to create a request with path match for completion
static httplib::Request make_completion_request(const std::string& job_id) {
    httplib::Request req;
    req.path = "/jobs/" + job_id + "/complete";
    std::smatch m;
    std::string path = req.path;
    std::regex pattern(R"(/jobs/([a-fA-F0-9\\-]{36})/complete)");
    // Note: The regex in handler is R"(/jobs/([a-fA-F0-9\\-]{36})/complete)"
    // We need to match that.
    // Let's use a simpler regex for testing if the handler uses the one passed in server.
    // Actually the handler doesn't know the regex, it just uses req.matches.
    // So we can use any regex that produces matches.
    std::regex test_pattern(R"(/jobs/(.+)/complete)");
    if (std::regex_match(path, m, test_pattern)) {
        req.matches = m;
    }
    return req;
}

TEST(JobHandlersTest, JobCompletionHandlerSuccess) {
    std::string job_id = "job-complete-1";
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        nlohmann::json job;
        job["job_id"] = job_id;
        job["status"] = "pending";
        jobs_db[job_id] = job;
    }
    
    auto auth = std::make_shared<AuthMiddleware>("");
    JobCompletionHandler handler(auth);
    httplib::Response res;
    auto req = make_completion_request(job_id);
    
    nlohmann::json body;
    body["output_url"] = "http://example.com/out.mp4";
    req.body = body.dump();
    
    handler.handle(req, res);
    EXPECT_EQ(res.status, 200);
    
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        EXPECT_EQ(jobs_db[job_id]["status"], "completed");
        EXPECT_EQ(jobs_db[job_id]["output_url"], "http://example.com/out.mp4");
    }
}

TEST(JobHandlersTest, JobAssignmentSuccess) {
    // Setup pending job and idle engine
    auto job_repo = std::make_shared<InMemoryJobRepository>();
    
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db.clear();
        engines_db.clear();
        
        nlohmann::json job;
        job["job_id"] = "job-assign-1";
        job["status"] = "pending";
        job["job_size"] = 10.0;
        job["priority"] = 0;
        job["created_at"] = 1000;
        jobs_db["job-assign-1"] = job;
        
        // Also save to repo because handler uses repo to find job
        job_repo->save_job("job-assign-1", job);
        
        nlohmann::json engine;
        engine["engine_id"] = "engine-idle";
        engine["status"] = "idle";
        engine["benchmark_time"] = 10.0;
        engines_db["engine-idle"] = engine;
    }
    
    auto auth = std::make_shared<AuthMiddleware>("");
    JobAssignmentHandler handler(auth, job_repo);
    httplib::Response res;
    httplib::Request req;
    
    nlohmann::json body;
    body["engine_id"] = "engine-idle";
    req.body = body.dump();
    
    handler.handle(req, res);
    EXPECT_EQ(res.status, 200);
    
    nlohmann::json parsed = nlohmann::json::parse(res.body);
    EXPECT_EQ(parsed["job_id"], "job-assign-1");
    EXPECT_EQ(parsed["assigned_engine"], "engine-idle");
    
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        EXPECT_EQ(jobs_db["job-assign-1"]["status"], "assigned");
        EXPECT_EQ(engines_db["engine-idle"]["status"], "busy");
    }
}
