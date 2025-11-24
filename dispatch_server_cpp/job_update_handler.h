#ifndef JOB_UPDATE_HANDLER_H
#define JOB_UPDATE_HANDLER_H

#include "request_handlers.h"
#include "repositories.h"
#include "nlohmann/json.hpp"
#include <memory>

namespace distconv {
namespace DispatchServer {

// Handler for PUT /jobs/{id} - Update job parameters
class JobUpdateHandler : public IRequestHandler {
public:
    explicit JobUpdateHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;

private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
};

// Handler for GET /engines/{id}/jobs - Get jobs for a specific engine
class EngineJobsHandler : public IRequestHandler {
public:
    explicit EngineJobsHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;

private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
};

// Handler for PUT/PATCH /jobs/{id}/status - Unified status update
class JobUnifiedStatusHandler : public IRequestHandler {
public:
    explicit JobUnifiedStatusHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;

private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
};

// Handler for POST /jobs/{id}/progress - Report job progress
class JobProgressHandler : public IRequestHandler {
public:
    explicit JobProgressHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IJobRepository> job_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;

private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
};

} // namespace DispatchServer
} // namespace distconv

#endif  // JOB_UPDATE_HANDLER_H
