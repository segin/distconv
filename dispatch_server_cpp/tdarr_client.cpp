#include "tdarr_client.h"
#include <iostream>

namespace distconv {
namespace Tdarr {

TdarrClient::TdarrClient(const std::string& base_url, const std::string& api_key)
    : base_url_(base_url), api_key_(api_key) {
    setup_client();
}

void TdarrClient::setup_client() {
    cli_ = std::make_unique<httplib::Client>(base_url_.c_str());
    cli_->set_connection_timeout(5); // 5 seconds timeout
    cli_->set_read_timeout(10);      // 10 seconds timeout
}

bool TdarrClient::submit_job(const nlohmann::json& job_data, std::string& out_tdarr_job_id) {
    if (!cli_) return false;

    // Tdarr API expects a specific format for job submission
    // This is a placeholder for the actual Tdarr API structure
    // Based on research, Tdarr v2 uses /api/v2/submit-job or similar endpoints
    nlohmann::json payload;
    payload["data"] = job_data;
    
    httplib::Headers headers;
    if (!api_key_.empty()) {
        headers.emplace("X-API-Key", api_key_);
    }

    auto res = cli_->Post("/api/v2/submit-job", headers, payload.dump(), "application/json");
    if (res && res->status == 200) {
        auto response_json = nlohmann::json::parse(res->body);
        if (response_json.contains("job_id")) {
            out_tdarr_job_id = response_json["job_id"];
            return true;
        }
    } else {
        std::cerr << "Tdarr job submission failed. StatusCode: " << (res ? std::to_string(res->status) : "connection error") << std::endl;
    }
    return false;
}

bool TdarrClient::check_health() {
    if (!cli_) return false;
    auto res = cli_->Get("/api/v2/status");
    return res && res->status == 200;
}

bool TdarrClient::get_job_status(const std::string& tdarr_job_id, nlohmann::json& out_status) {
    if (!cli_) return false;
    auto res = cli_->Get(("/api/v2/job-status/" + tdarr_job_id).c_str());
    if (res && res->status == 200) {
        out_status = nlohmann::json::parse(res->body);
        return true;
    }
    return false;
}

} // namespace Tdarr
} // namespace distconv
