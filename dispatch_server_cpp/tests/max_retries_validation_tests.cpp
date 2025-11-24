#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "job_handlers.h"
#include "dispatch_server_core.h" // For global state if needed, though we mock auth

// Mock AuthMiddleware to always return true
class MockAuthMiddleware : public AuthMiddleware {
public:
    MockAuthMiddleware() : AuthMiddleware("") {}
    bool authenticate(const httplib::Request&, httplib::Response&) const override {
        return true;
    }
};

class MaxRetriesValidationTest : public ::testing::Test {
protected:
    std::shared_ptr<MockAuthMiddleware> auth;
    std::shared_ptr<JobSubmissionHandler> handler;

    void SetUp() override {
        auth = std::make_shared<MockAuthMiddleware>();
        handler = std::make_shared<JobSubmissionHandler>(auth);
    }
};

TEST_F(MaxRetriesValidationTest, ValidMaxRetries) {
    httplib::Request req;
    httplib::Response res;
    
    nlohmann::json body;
    body["source_url"] = "http://example.com/video.mp4";
    body["target_codec"] = "h264";
    body["max_retries"] = 5;
    req.body = body.dump();
    
    // We need to inspect the response or the created job. 
    // Since create_job is private/protected or called internally, we check the response.
    // JobSubmissionHandler::handle calls create_job and returns 200 on success.
    
    handler->handle(req, res);
    
    EXPECT_EQ(res.status, 200);
    nlohmann::json response_body = nlohmann::json::parse(res.body);
    EXPECT_EQ(response_body["max_retries"], 5);
}

TEST_F(MaxRetriesValidationTest, InvalidMaxRetriesNegative) {
    httplib::Request req;
    httplib::Response res;
    
    nlohmann::json body;
    body["source_url"] = "http://example.com/video.mp4";
    body["target_codec"] = "h264";
    body["max_retries"] = -1;
    req.body = body.dump();
    
    handler->handle(req, res);
    
    EXPECT_EQ(res.status, 400);
    nlohmann::json response_body = nlohmann::json::parse(res.body);
    EXPECT_EQ(response_body["error_type"], "validation_error");
    EXPECT_NE(response_body["error"].get<std::string>().find("non-negative"), std::string::npos);
}

TEST_F(MaxRetriesValidationTest, InvalidMaxRetriesType) {
    httplib::Request req;
    httplib::Response res;
    
    nlohmann::json body;
    body["source_url"] = "http://example.com/video.mp4";
    body["target_codec"] = "h264";
    body["max_retries"] = "five"; // String instead of int
    req.body = body.dump();
    
    handler->handle(req, res);
    
    EXPECT_EQ(res.status, 400);
    nlohmann::json response_body = nlohmann::json::parse(res.body);
    EXPECT_EQ(response_body["error_type"], "validation_error");
    EXPECT_NE(response_body["error"].get<std::string>().find("must be an integer"), std::string::npos);
}

TEST_F(MaxRetriesValidationTest, MissingMaxRetriesUsesDefault) {
    httplib::Request req;
    httplib::Response res;
    
    nlohmann::json body;
    body["source_url"] = "http://example.com/video.mp4";
    body["target_codec"] = "h264";
    // max_retries missing
    req.body = body.dump();
    
    handler->handle(req, res);
    
    EXPECT_EQ(res.status, 200);
    nlohmann::json response_body = nlohmann::json::parse(res.body);
    // Default is usually 3, defined in dispatch_server_constants.h
    // We can check if it's present and a number
    EXPECT_TRUE(response_body.contains("max_retries"));
    EXPECT_TRUE(response_body["max_retries"].is_number_integer());
}
