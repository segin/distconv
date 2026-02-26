#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "../repositories.h"
#include <thread>
#include <chrono>
#include <random>

using namespace distconv::DispatchServer;

// Helper from test_common.h (copying to avoid linking issues or dependency on test_common structure)
inline int find_available_port_modern() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(20000, 65000);
    return dis(gen);
}

class ModernApiTest : public ::testing::Test {
protected:
    std::shared_ptr<DispatchServer> server;
    std::shared_ptr<httplib::Client> client;
    int port;
    std::string api_key = "test_api_key";
    std::shared_ptr<InMemoryJobRepository> job_repo;

    void SetUp() override {
        port = find_available_port_modern();
        job_repo = std::make_shared<InMemoryJobRepository>();
        auto engine_repo = std::make_shared<InMemoryEngineRepository>();

        server = std::make_shared<DispatchServer>(job_repo, engine_repo, api_key);
        server->start(port, false);

        // Wait for server
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Populate jobs
        for(int i=0; i<150; ++i) {
            nlohmann::json job;
            job["job_id"] = "job_" + std::to_string(i);
            // created_at descending order is default in implementation
            job["created_at"] = 1000 + i;
            job["status"] = "pending";
            job_repo->save_job(job["job_id"], job);
        }

        client = std::make_shared<httplib::Client>("localhost", port);
        client->set_connection_timeout(5);
    }

    void TearDown() override {
        if (server) server->stop();
    }
};

TEST_F(ModernApiTest, PaginationDefaults) {
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Get("/jobs/", headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    auto json = nlohmann::json::parse(res->body);
    ASSERT_EQ(json.size(), 100); // Default limit
}

TEST_F(ModernApiTest, PaginationCustomLimit) {
    httplib::Headers headers = {{"X-API-Key", api_key}};
    auto res = client->Get("/jobs/?limit=50", headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    auto json = nlohmann::json::parse(res->body);
    ASSERT_EQ(json.size(), 50);
}

TEST_F(ModernApiTest, PaginationOffset) {
    httplib::Headers headers = {{"X-API-Key", api_key}};
    // offset 100, should get remaining 50.
    // Note: InMemoryJobRepository sorts by created_at DESC.
    // 0..149.
    // Offset 0 (first 100): 149..50
    // Offset 100 (next 50): 49..0
    auto res = client->Get("/jobs/?offset=100", headers);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
    auto json = nlohmann::json::parse(res->body);
    ASSERT_EQ(json.size(), 50);
}
