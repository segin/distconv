#ifndef SUBMISSION_CLIENT_CORE_H
#define SUBMISSION_CLIENT_CORE_H

#include <string>
#include <vector>
#include "nlohmann/json.hpp"

#include "cpr/cpr.h"

class CprApi {
public:
    virtual ~CprApi() = default;
    virtual cpr::Response Post(const cpr::Url& url, const cpr::Header& header, const cpr::Body& body, const cpr::SslOptions& ssl_opts) {
        return cpr::Post(url, header, body, ssl_opts);
    }
};

class ApiClient {
public:
    ApiClient(const std::string& server_url, const std::string& api_key, const std::string& ca_cert_path, std::unique_ptr<CprApi> cpr_api = std::make_unique<CprApi>());

    nlohmann::json submitJob(const std::string& source_url, const std::string& target_codec, double job_size, int max_retries);
    nlohmann::json getJobStatus(const std::string& job_id);
    nlohmann::json listAllJobs();
    nlohmann::json listAllEngines();

private:
    std::string server_url_;
    std::string api_key_;
    std::string ca_cert_path_;
    std::unique_ptr<CprApi> cpr_api_;
};

// Global configuration (will be set via UI/command line)
extern std::string g_dispatchServerUrl;
extern std::string g_apiKey;
extern std::string g_caCertPath;

// Function declarations
void saveJobId(const std::string& job_id);
std::vector<std::string> loadJobIds();

// Main application function
int run_submission_client(int argc, char* argv[]);

#endif // SUBMISSION_CLIENT_CORE_H
