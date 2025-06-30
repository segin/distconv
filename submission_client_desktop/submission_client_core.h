#ifndef SUBMISSION_CLIENT_CORE_H
#define SUBMISSION_CLIENT_CORE_H

#include <string>
#include <vector>
#include "nlohmann/json.hpp"

// Global configuration (will be set via UI/command line)
extern std::string g_dispatchServerUrl;
extern std::string g_apiKey;
extern std::string g_caCertPath;

// Function declarations
void saveJobId(const std::string& job_id);
std::vector<std::string> loadJobIds();
void submitJob(const std::string& source_url, const std::string& target_codec, double job_size, int max_retries);
void getJobStatus(const std::string& job_id);
void listAllJobs();
void listAllEngines();

// Main application function
int run_submission_client(int argc, char* argv[]);

#endif // SUBMISSION_CLIENT_CORE_H
