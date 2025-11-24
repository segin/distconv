#include <gtest/gtest.h>
#include "job_action_handlers.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "dispatch_server_core.h" // For global state if needed, though we try to avoid it

using namespace distconv::DispatchServer;

class URLValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear global state
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db.clear();
        engines_db.clear();
    }
    
    void TearDown() override {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db.clear();
        engines_db.clear();
    }
};

TEST_F(URLValidationTest, ValidURL) {
    std::cout << "TEST_F(URLValidationTest, ValidURL) {" << std::endl;
    std::cout << "Starting ValidURL test" << std::endl;
    auto auth = std::make_shared<AuthMiddleware>("");
    JobCompletionHandler handler(auth);
    
    // Setup job
    std::string job_id = "job-1";
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db[job_id] = {{"job_id", job_id}, {"status", "assigned"}};
    }
    
    httplib::Request req;
    req.matches = std::smatch(); // We need to fake matches, but httplib::Request matches is not easily settable without regex_match
    // Workaround: The handler uses req.matches[1] for job_id. 
    // We can't easily populate std::smatch directly. 
    // However, we can use the same trick as in other tests: use std::regex_match to populate it.
    std::string path = "/jobs/" + job_id + "/complete";
    std::regex re(R"(/jobs/(.+)/complete)");
    std::regex_match(path, req.matches, re);
    
    nlohmann::json body;
    body["output_url"] = "https://example.com/output.mp4";
    req.body = body.dump();
    
    httplib::Response res;
    handler.handle(req, res);
    
    EXPECT_EQ(res.status, 200);
    
    // Verify DB update
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        EXPECT_EQ(jobs_db[job_id]["status"], "completed");
        EXPECT_EQ(jobs_db[job_id]["output_url"], "https://example.com/output.mp4");
    }
}

TEST_F(URLValidationTest, InvalidURLNoProtocol) {
    std::cout << "Starting InvalidURLNoProtocol test" << std::endl;
    auto auth = std::make_shared<AuthMiddleware>("");
    JobCompletionHandler handler(auth);
    
    std::string job_id = "job-1";
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db[job_id] = {{"job_id", job_id}, {"status", "assigned"}};
    }
    
    httplib::Request req;
    std::string path = "/jobs/" + job_id + "/complete";
    std::regex re(R"(/jobs/(.+)/complete)");
    std::regex_match(path, req.matches, re);
    
    nlohmann::json body;
    body["output_url"] = "example.com/output.mp4"; // Missing https://
    req.body = body.dump();
    
    httplib::Response res;
    handler.handle(req, res);
    
    EXPECT_EQ(res.status, 400);
    nlohmann::json error = nlohmann::json::parse(res.body);
    EXPECT_EQ(error["error_type"], "validation_error");
    EXPECT_TRUE(error["error"].get<std::string>().find("valid URL") != std::string::npos);
}

TEST_F(URLValidationTest, InvalidURLFtpProtocol) {
    std::cout << "Starting InvalidURLFtpProtocol test" << std::endl;
    auto auth = std::make_shared<AuthMiddleware>("");
    JobCompletionHandler handler(auth);
    
    std::string job_id = "job-1";
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db[job_id] = {{"job_id", job_id}, {"status", "assigned"}};
    }
    
    httplib::Request req;
    std::string path = "/jobs/" + job_id + "/complete";
    std::regex re(R"(/jobs/(.+)/complete)");
    std::regex_match(path, req.matches, re);
    
    nlohmann::json body;
    body["output_url"] = "ftp://example.com/output.mp4"; // Wrong protocol
    req.body = body.dump();
    
    httplib::Response res;
    handler.handle(req, res);
    
    EXPECT_EQ(res.status, 400);
}
