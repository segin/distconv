#ifndef ASSIGNMENT_HANDLER_H
#define ASSIGNMENT_HANDLER_H

#include "request_handlers.h"
#include "nlohmann/json.hpp"
#include "repositories.h"
#include <string>

// Handler for POST /assign_job/ - Assign a job to an engine
class JobAssignmentHandler : public IRequestHandler {
public:
    explicit JobAssignmentHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;

private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
};

#endif  // ASSIGNMENT_HANDLER_H
