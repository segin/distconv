#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h" // For ApiTest fixture and clear_db()

TEST_F(ApiTest, SubmitValidJob) {
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    job_payload["job_size"] = 100.5;
    job_payload["max_retries"] = 5;

    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.contains("job_id"));
    ASSERT_EQ(response_json["source_url"], "http://example.com/video.mp4");
    ASSERT_EQ(response_json["target_codec"], "h264");
    ASSERT_EQ(response_json["job_size"], 100.5);
    ASSERT_EQ(response_json["status"], "pending");
    ASSERT_EQ(response_json["max_retries"], 5);

    // Verify job is in the database
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        ASSERT_TRUE(jobs_db.contains(response_json["job_id"]));
    }
}