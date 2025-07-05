#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h" // For ApiTest fixture and clear_db()

TEST_F(ApiTest, GetJobStatusValid) {
    // First, submit a job to have something to query
    nlohmann::json job_payload;
    job_payload["source_url"] = "http://example.com/video.mp4";
    job_payload["target_codec"] = "h264";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto post_res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(post_res->status, 200);
    nlohmann::json post_response_json = nlohmann::json::parse(post_res->body);
    std::string job_id = post_response_json["job_id"];

    // Now, get the job status
    auto get_res = client->Get(("/jobs/" + job_id).c_str(), headers);

    ASSERT_TRUE(get_res != nullptr);
    ASSERT_EQ(get_res->status, 200);

    nlohmann::json get_response_json = nlohmann::json::parse(get_res->body);
    ASSERT_EQ(get_response_json["job_id"], job_id);
    ASSERT_EQ(get_response_json["status"], "pending");
}
