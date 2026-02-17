#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "test_common.h"
#include "../dispatch_server_core.h"
#include "../repositories.h"

using namespace distconv::DispatchServer;

class ModernApiTest : public ApiTest {
protected:
    static void SetUpTestSuite() {
        use_legacy_mode = false; // Enable Modern Mode (SQLite)
        ApiTest::SetUpTestSuite();
    }
    static void TearDownTestSuite() {
        ApiTest::TearDownTestSuite();
        use_legacy_mode = true; // Restore default for other tests
    }
};

TEST_F(ModernApiTest, SubmitJobSavesToSqliteWithPriority) {
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"priority", 5}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    // Modern API returns 201 Created
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 201);

    auto json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(json.contains("job_id"));
    std::string job_id = json["job_id"];

    // Verify via repository directly (bypassing legacy jobs_db)
    auto repo = server->get_job_repository();
    ASSERT_TRUE(repo != nullptr);
    ASSERT_TRUE(repo->job_exists(job_id));

    auto job = repo->get_job(job_id);
    ASSERT_EQ(job["priority"], 5);
    ASSERT_EQ(job["source_url"], "http://example.com/video.mp4");
}

TEST_F(ModernApiTest, EngineHeartbeatUpdatesRepository) {
    nlohmann::json engine_payload = {
        {"engine_id", "modern-engine-1"},
        {"status", "idle"}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    auto repo = server->get_engine_repository();
    ASSERT_TRUE(repo != nullptr);
    ASSERT_TRUE(repo->engine_exists("modern-engine-1"));

    auto engine = repo->get_engine("modern-engine-1");
    ASSERT_EQ(engine["status"], "idle");
}
