#ifndef TDARR_CLIENT_H
#define TDARR_CLIENT_H

#include <string>
#include <memory>
#include "nlohmann/json.hpp"
#include "httplib.h"

namespace distconv {
namespace Tdarr {

class TdarrClient {
public:
    TdarrClient(const std::string& base_url, const std::string& api_key = "");
    virtual ~TdarrClient() = default;

    // Submits a transcoding job to Tdarr
    virtual bool submit_job(const nlohmann::json& job_data, std::string& out_tdarr_job_id);

    // Checks the health of the Tdarr server
    virtual bool check_health();

    // Gets the status of a specific job from Tdarr
    virtual bool get_job_status(const std::string& tdarr_job_id, nlohmann::json& out_status);

private:
    std::string base_url_;
    std::string api_key_;
    std::unique_ptr<httplib::Client> cli_;

    void setup_client();
};

} // namespace Tdarr
} // namespace distconv

#endif // TDARR_CLIENT_H
