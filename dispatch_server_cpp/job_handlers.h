#ifndef JOB_HANDLERS_H
#define JOB_HANDLERS_H

#include "request_handlers.h"
#include "nlohmann/json.hpp"
#include <string>

// Handler for POST /jobs/ - Job submission
class JobSubmissionHandler : public IRequestHandler {
public:
    explicit JobSubmissionHandler(std::shared_ptr<AuthMiddleware> auth);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    
    // Validation helper
    bool validate_job_input(const nlohmann::json& input, httplib::Response& res);
    
    // Job creation helper
    nlohmann::json create_job(const nlohmann::json& input);
};

#endif  // JOB_HANDLERS_H
