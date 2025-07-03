#include "gtest/gtest.h"
#include "../submission_client_core.h" // Include the header for the core logic
#include "mock_cpr.h"
#include "trompeloeil.hpp"
#include <fstream>
#include <string>
#include <vector>

class MockCprApi : public CprApi {
public:
    MAKE_MOCK4(Post, cpr::Response(const cpr::Url&, const cpr::Header&, const cpr::Body&, const cpr::SslOptions&));
};

// Test fixture for the Submission Client
class SubmissionClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize any necessary client components or mock dependencies
        // Reset global state for each test to ensure isolation
        g_dispatchServerUrl = "http://localhost:8080";
        g_apiKey = "test_api_key";
        g_caCertPath = "";

        // Clear the job IDs file for each test
        std::ofstream ofs("submitted_job_ids.txt", std::ios_base::trunc);
        if (ofs.is_open()) {
            ofs.close();
        }
    }

    void TearDown() override {
        // Clean up resources
        remove("submitted_job_ids.txt");
    }
};

TEST_F(SubmissionClientTest, SubmitJobSendsPostRequestToCorrectUrl) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"({"job_id": "123"})";

    REQUIRE_CALL(*mock_cpr_api, Post(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    apiClient.submitJob("http://example.com/video.mp4", "h264", 100.0, 3);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
