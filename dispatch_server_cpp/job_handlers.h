#ifndef JOB_HANDLERS_H
#define JOB_HANDLERS_H

#include "request_handlers.h"
#include "nlohmann/json.hpp"
#include "repositories.h"
#include <string>
#include <memory>

namespace distconv {
namespace DispatchServer {

// Handler for POST /jobs/ - Job submission
class JobSubmissionHandler : public IRequestHandler {
public:
    JobSubmissionHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
    
    // Validation helper
    bool validate_job_input(const nlohmann::json& input, httplib::Response& res);
    
    // Job creation helper
    nlohmann::json create_job(const nlohmann::json& input);
};

// Handler for GET /jobs/{id} - Get job status
class JobStatusHandler : public IRequestHandler {
public:
    JobStatusHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
};

// Handler for GET /jobs/ - List all jobs
class JobListHandler : public IRequestHandler {
public:
    JobListHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
};

// Handler for POST /jobs/{id}/retry
class JobRetryHandler : public IRequestHandler {
public:
    JobRetryHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
};

// Handler for POST /jobs/{id}/cancel
class JobCancelHandler : public IRequestHandler {
public:
    JobCancelHandler(std::shared_ptr<AuthMiddleware> auth, 
                     std::shared_ptr<IJobRepository> job_repo,
                     std::shared_ptr<IEngineRepository> engine_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
    std::shared_ptr<IEngineRepository> engine_repo_;
};

} // namespace DispatchServer
} // namespace distconv

#endif  // JOB_HANDLERS_H
