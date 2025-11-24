#ifndef JOB_ACTION_HANDLERS_H
#define JOB_ACTION_HANDLERS_H

#include "request_handlers.h"
#include "nlohmann/json.hpp"
#include <string>

// Handler for POST /jobs/{id}/complete - Mark job as completed
class JobCompletionHandler : public IRequestHandler {
public:
    explicit JobCompletionHandler(std::shared_ptr<AuthMiddleware> auth);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
};

// Handler for POST /jobs/{id}/fail - Mark job as failed
class JobFailureHandler : public IRequestHandler {
public:
    explicit JobFailureHandler(std::shared_ptr<AuthMiddleware> auth);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
};

#endif  // JOB_ACTION_HANDLERS_H
