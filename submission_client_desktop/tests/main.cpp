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

        // Use a temporary file for job IDs during tests
        temp_job_ids_file_ = "test_submitted_job_ids.txt";
        JOB_IDS_FILE = temp_job_ids_file_;

        // Clear the job IDs file for each test
        std::ofstream ofs(JOB_IDS_FILE, std::ios_base::trunc);
        if (ofs.is_open()) {
            ofs.close();
        }
    }

    void TearDown() override {
        // Clean up resources
        remove(JOB_IDS_FILE.c_str());
    }

    std::string temp_job_ids_file_;
};

TEST_F(SubmissionClientTest, SubmitJobSendsPostRequestToCorrectUrl) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"({"job_id": "123"})";

    REQUIRE_CALL(*mock_cpr_api, Post(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::capture(header_arg), trompeloeil::capture(body_arg), trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    std::string expected_source_url = "http://example.com/video.mp4";
    std::string expected_target_codec = "h264";
    double expected_job_size = 100.0;
    int expected_max_retries = 3;
    nlohmann::json result_json = apiClient.submitJob(expected_source_url, expected_target_codec, expected_job_size, expected_max_retries);

    nlohmann::json sent_payload = nlohmann::json::parse(body_arg.str());
    ASSERT_EQ(sent_payload["source_url"], expected_source_url);
    ASSERT_EQ(sent_payload["target_codec"], expected_target_codec);
    ASSERT_EQ(sent_payload["job_size"], expected_job_size);
    ASSERT_EQ(sent_payload["max_retries"], expected_max_retries);
    ASSERT_EQ(header_arg["X-API-Key"], g_apiKey);
    ASSERT_EQ(header_arg["Content-Type"], "application/json");
    ASSERT_TRUE(result_json.contains("job_id"));
    ASSERT_EQ(result_json["job_id"], "123");
}

TEST_F(SubmissionClientTest, SubmitJobCallsSaveJobIdOnSuccess) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"({"job_id": "test_job_id_456"})";

    REQUIRE_CALL(*mock_cpr_api, Post(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    apiClient.submitJob("http://example.com/video.mp4", "h264", 100.0, 3);

    std::vector<std::string> saved_job_ids = loadJobIds();
    ASSERT_EQ(saved_job_ids.size(), 1);
    ASSERT_EQ(saved_job_ids[0], "test_job_id_456");
}

TEST_F(SubmissionClientTest, SubmitJobUsesSslVerificationWhenCaPathProvided) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"({"job_id": "123"})";

    std::string expected_ca_path = "/path/to/ca.crt";
    g_caCertPath = expected_ca_path;

    REQUIRE_CALL(*mock_cpr_api, Post(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::_, trompeloeil::capture(ssl_opts_arg)))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    apiClient.submitJob("http://example.com/video.mp4", "h264", 100.0, 3);

    ASSERT_EQ(ssl_opts_arg.ca_info, expected_ca_path);
    ASSERT_TRUE(ssl_opts_arg.verify_peer);
    ASSERT_TRUE(ssl_opts_arg.verify_host);
}

TEST_F(SubmissionClientTest, SubmitJobDisablesSslVerificationWhenNoCaPathProvided) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"({"job_id": "123"})";

    g_caCertPath = ""; // Ensure no CA path is provided

    REQUIRE_CALL(*mock_cpr_api, Post(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::_, trompeloeil::capture(ssl_opts_arg)))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    apiClient.submitJob("http://example.com/video.mp4", "h264", 100.0, 3);

    ASSERT_FALSE(ssl_opts_arg.verify_peer);
    ASSERT_FALSE(ssl_opts_arg.verify_host);
}

TEST_F(SubmissionClientTest, SubmitJobHandlesServerError) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 500;
    response.text = "Internal Server Error";

    REQUIRE_CALL(*mock_cpr_api, Post(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    ASSERT_THROW({
        apiClient.submitJob("http://example.com/video.mp4", "h264", 100.0, 3);
    }, std::runtime_error);
}

TEST_F(SubmissionClientTest, SubmitJobHandlesClientError) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 400;
    response.text = "Bad Request: Missing parameter";

    REQUIRE_CALL(*mock_cpr_api, Post(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    ASSERT_THROW({
        apiClient.submitJob("http://example.com/video.mp4", "h264", 100.0, 3);
    }, std::runtime_error);
}

TEST_F(SubmissionClientTest, SubmitJobHandlesTransportError) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 0; // Indicates a transport error
    response.error.code = cpr::Error::codes::HostLookupError;
    response.error.message = "Could not resolve host";

    REQUIRE_CALL(*mock_cpr_api, Post(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    ASSERT_THROW({
        apiClient.submitJob("http://example.com/video.mp4", "h264", 100.0, 3);
    }, std::runtime_error);
}

TEST_F(SubmissionClientTest, GetJobStatusHandlesSuccessfulResponse) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"({"job_id": "test_job_id_123", "status": "pending"})";

    std::string job_id = "test_job_id_123";

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/" + job_id}, trompeloeil::capture(header_arg), trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    std::string job_id = "test_job_id_123";
    nlohmann::json result_json = apiClient.getJobStatus(job_id);

    ASSERT_EQ(header_arg["X-API-Key"], g_apiKey);
    ASSERT_TRUE(result_json.contains("job_id"));
    ASSERT_EQ(result_json["job_id"], job_id);
    ASSERT_TRUE(result_json.contains("status"));
    ASSERT_EQ(result_json["status"], "pending");
}

TEST_F(SubmissionClientTest, GetJobStatusHandlesNotFoundResponse) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 404;
    response.text = "Job not found";

    std::string job_id = "non_existent_job_id";

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/" + job_id}, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    ASSERT_THROW({
        apiClient.getJobStatus(job_id);
    }, std::runtime_error);
}

TEST_F(SubmissionClientTest, GetJobStatusHandlesServerError) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 500;
    response.text = "Internal Server Error";

    std::string job_id = "test_job_id_123";

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/" + job_id}, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    ASSERT_THROW({
        apiClient.getJobStatus(job_id);
    }, std::runtime_error);
}

TEST_F(SubmissionClientTest, GetJobStatusHandlesTransportError) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 0; // Indicates a transport error
    response.error.code = cpr::Error::codes::HostLookupError;
    response.error.message = "Could not resolve host";

    std::string job_id = "test_job_id_123";

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/" + job_id}, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    ASSERT_THROW({
        apiClient.getJobStatus(job_id);
    }, std::runtime_error);
}

TEST_F(SubmissionClientTest, GetJobStatusUsesSslVerificationWhenCaPathProvided) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"({"job_id": "test_job_id_123", "status": "pending"})";

    std::string expected_ca_path = "/path/to/ca.crt";
    g_caCertPath = expected_ca_path;

    std::string job_id = "test_job_id_123";

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/" + job_id}, trompeloeil::_, trompeloeil::capture(ssl_opts_arg)))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    apiClient.getJobStatus(job_id);

    ASSERT_EQ(ssl_opts_arg.ca_info, expected_ca_path);
    ASSERT_TRUE(ssl_opts_arg.verify_peer);
    ASSERT_TRUE(ssl_opts_arg.verify_host);
}

TEST_F(SubmissionClientTest, GetJobStatusDisablesSslVerificationWhenNoCaPathProvided) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"({"job_id": "test_job_id_123", "status": "pending"})";

    g_caCertPath = ""; // Ensure no CA path is provided

    std::string job_id = "test_job_id_123";

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/" + job_id}, trompeloeil::_, trompeloeil::capture(ssl_opts_arg)))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    apiClient.getJobStatus(job_id);

    ASSERT_FALSE(ssl_opts_arg.verify_peer);
    ASSERT_FALSE(ssl_opts_arg.verify_host);
}

TEST_F(SubmissionClientTest, ListAllJobsSendsGetRequestToCorrectUrl) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"([{"job_id": "job1", "status": "pending"}, {"job_id": "job2", "status": "completed"}])";

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::capture(header_arg), trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    nlohmann::json result_json = apiClient.listAllJobs();

    ASSERT_EQ(header_arg["X-API-Key"], g_apiKey);
    ASSERT_TRUE(result_json.is_array());
    ASSERT_EQ(result_json.size(), 2);
    ASSERT_EQ(result_json[0]["job_id"], "job1");
    ASSERT_EQ(result_json[1]["job_id"], "job2");
}

TEST_F(SubmissionClientTest, ListAllJobsHandlesSuccessfulResponse) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"([{"job_id": "job1", "status": "pending"}, {"job_id": "job2", "status": "completed"}])";

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    nlohmann::json result_json = apiClient.listAllJobs();

    ASSERT_TRUE(result_json.is_array());
    ASSERT_EQ(result_json.size(), 2);
    ASSERT_EQ(result_json[0]["job_id"], "job1");
    ASSERT_EQ(result_json[0]["status"], "pending");
    ASSERT_EQ(result_json[1]["job_id"], "job2");
    ASSERT_EQ(result_json[1]["status"], "completed");
}

TEST_F(SubmissionClientTest, ListAllJobsHandlesServerError) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 500;
    response.text = "Internal Server Error";

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    ASSERT_THROW({
        apiClient.listAllJobs();
    }, std::runtime_error);
}

TEST_F(SubmissionClientTest, ListAllJobsHandlesTransportError) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 0; // Indicates a transport error
    response.error.code = cpr::Error::codes::HostLookupError;
    response.error.message = "Could not resolve host";

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::_))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    ASSERT_THROW({
        apiClient.listAllJobs();
    }, std::runtime_error);
}

TEST_F(SubmissionClientTest, ListAllJobsUsesSslVerificationWhenCaPathProvided) {
    auto mock_cpr_api = std::make_unique<MockCprApi>();
    
    cpr::Response response;
    response.status_code = 200;
    response.text = R"([{"job_id": "job1", "status": "pending"}])";

    std::string expected_ca_path = "/path/to/ca.crt";
    g_caCertPath = expected_ca_path;

    REQUIRE_CALL(*mock_cpr_api, Get(cpr::Url{g_dispatchServerUrl + "/jobs/"}, trompeloeil::_, trompeloeil::capture(ssl_opts_arg)))
        .LR_RETURN(response);

    ApiClient apiClient(g_dispatchServerUrl, g_apiKey, g_caCertPath, std::move(mock_cpr_api));

    apiClient.listAllJobs();

    ASSERT_EQ(ssl_opts_arg.ca_info, expected_ca_path);
    ASSERT_TRUE(ssl_opts_arg.verify_peer);
    ASSERT_TRUE(ssl_opts_arg.verify_host);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}