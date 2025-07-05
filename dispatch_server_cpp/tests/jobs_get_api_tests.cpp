
TEST_F(ApiTest, ListAllJobsWithJobs) {
    clear_db();
    // Submit two jobs
    nlohmann::json job1_payload;
    job1_payload["source_url"] = "http://example.com/video1.mp4";
    job1_payload["target_codec"] = "h264";
    httplib::Headers headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    client->Post("/jobs/", headers, job1_payload.dump(), "application/json");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    nlohmann::json job2_payload;
    job2_payload["source_url"] = "http://example.com/video2.mp4";
    job2_payload["target_codec"] = "vp9";
    client->Post("/jobs/", headers, job2_payload.dump(), "application/json");

    auto res = client->Get("/jobs/", headers);

    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);

    nlohmann::json response_json = nlohmann::json::parse(res->body);
    ASSERT_TRUE(response_json.is_array());
    ASSERT_EQ(response_json.size(), 2);

    // Check some details of the returned jobs
    ASSERT_EQ(response_json[0]["source_url"], "http://example.com/video1.mp4");
    ASSERT_EQ(response_json[1]["source_url"], "http://example.com/video2.mp4");
}