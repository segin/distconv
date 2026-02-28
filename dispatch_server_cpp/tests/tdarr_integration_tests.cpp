#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "tdarr_client.h"
#include "dispatch_server_core.h"
#include "repositories.h"

using namespace distconv::DispatchServer;
using namespace distconv::Tdarr;
using namespace testing;

class MockIJobRepository : public IJobRepository {
public:
    MOCK_METHOD(void, save_job, (const std::string&, const nlohmann::json&), (override));
    MOCK_METHOD(nlohmann::json, get_job, (const std::string&), (override));
    MOCK_METHOD(bool, job_exists, (const std::string&), (override));
    MOCK_METHOD(std::vector<nlohmann::json>, get_all_jobs, (), (override));
    MOCK_METHOD(void, remove_job, (const std::string&), (override));
    MOCK_METHOD(std::vector<nlohmann::json>, get_jobs_by_status, (const std::string&), (override));
    MOCK_METHOD(std::vector<nlohmann::json>, get_jobs_to_timeout, (int64_t), (override));
    MOCK_METHOD(std::vector<std::string>, get_stale_pending_jobs, (int64_t), (override));
    MOCK_METHOD(int, get_job_count, (), (override));
};

class MockIEngineRepository : public IEngineRepository {
public:
    MOCK_METHOD(void, save_engine, (const std::string&, const nlohmann::json&), (override));
    MOCK_METHOD(nlohmann::json, get_engine, (const std::string&), (override));
    MOCK_METHOD(bool, engine_exists, (const std::string&), (override));
    MOCK_METHOD(std::vector<nlohmann::json>, get_all_engines, (), (override));
    MOCK_METHOD(void, remove_engine, (const std::string&), (override));
    MOCK_METHOD(int, get_engine_count, (), (override));
};

TEST(TdarrIntegrationTest, ServerStatusEndpoint) {
    auto job_repo = std::make_shared<MockIJobRepository>();
    auto engine_repo = std::make_shared<MockIEngineRepository>();
    DispatchServer server(job_repo, engine_repo, "test-api-key");
    
    // We cannot easily mock the internal TdarrClient without more refactoring
    // But we can test that the endpoint exists and responds
    server.start(0, false);
    int port = server.get_port();
    
    httplib::Client cli("localhost", port);
    httplib::Headers headers = {{"X-API-Key", "test-api-key"}};
    
    auto res = cli.Get("/tdarr/status", headers);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    
    auto j = nlohmann::json::parse(res->body);
    EXPECT_TRUE(j.contains("tdarr_server"));
    
    server.stop();
}

TEST(TdarrIntegrationTest, SubmitEndpointUnauthorized) {
    auto job_repo = std::make_shared<MockIJobRepository>();
    auto engine_repo = std::make_shared<MockIEngineRepository>();
    DispatchServer server(job_repo, engine_repo, "test-api-key");
    
    server.start(0, false);
    int port = server.get_port();
    
    httplib::Client cli("localhost", port);
    auto res = cli.Post("/tdarr/submit", "{}", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
    
    server.stop();
}
