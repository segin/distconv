#ifndef JOB_ACTION_HANDLERS_H
#define JOB_ACTION_HANDLERS_H

#include "request_handlers.h"
#include "nlohmann/json.hpp"
#include "repositories.h"
#include <string>
#include <memory>

namespace distconv {
namespace DispatchServer {

// Handler for POST /jobs/{id}/complete - Mark job as completed
class JobCompletionHandler : public IRequestHandler {
public:
    JobCompletionHandler(std::shared_ptr<AuthMiddleware> auth, 
                         std::shared_ptr<IJobRepository> job_repo,
                         std::shared_ptr<IEngineRepository> engine_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IJobRepository> job_repo_;
    std::shared_ptr<IEngineRepository> engine_repo_;
};

// Handler for POST /jobs/{id}/fail - Mark job as failed
class JobFailureHandler : public IRequestHandler {
public:
    JobFailureHandler(std::shared_ptr<AuthMiddleware> auth, 
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

#endif  // JOB_ACTION_HANDLERS_H
