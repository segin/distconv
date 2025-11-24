#ifndef ASSIGNMENT_HANDLER_H
#define ASSIGNMENT_HANDLER_H

#include "request_handlers.h"
#include "nlohmann/json.hpp"
#include <string>

// Handler for POST /assign_job/ - Assign a job to an engine
class JobAssignmentHandler : public IRequestHandler {
public:
    explicit JobAssignmentHandler(std::shared_ptr<AuthMiddleware> auth);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    
    // Helper methods for assignment logic
    nlohmann::json find_pending_job();
    std::vector<nlohmann::json> find_available_engines();
    nlohmann::json select_engine_for_job(const nlohmann::json& job, const std::vector<nlohmann::json>& engines);
    void assign_job_to_engine(const nlohmann::json& job, const nlohmann::json& engine);
};

#endif  // ASSIGNMENT_HANDLER_H
