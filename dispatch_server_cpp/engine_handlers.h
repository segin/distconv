#ifndef ENGINE_HANDLERS_H
#define ENGINE_HANDLERS_H

#include "request_handlers.h"
#include "nlohmann/json.hpp"
#include "repositories.h"
#include <string>
#include <memory>

namespace distconv {
namespace DispatchServer {

// Handler for GET /engines/ - List all engines
class EngineListHandler : public IRequestHandler {
public:
    EngineListHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IEngineRepository> engine_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IEngineRepository> engine_repo_;
};

// Handler for POST /engines/heartbeat - Engine heartbeat
class EngineHeartbeatHandler : public IRequestHandler {
public:
    EngineHeartbeatHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IEngineRepository> engine_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IEngineRepository> engine_repo_;
    bool validate_heartbeat_input(const nlohmann::json& input, httplib::Response& res);
};

// Handler for POST /engines/benchmark_result - Engine benchmark result
class EngineBenchmarkHandler : public IRequestHandler {
public:
    EngineBenchmarkHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IEngineRepository> engine_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;
    
private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IEngineRepository> engine_repo_;
};

} // namespace DispatchServer
} // namespace distconv

#endif  // ENGINE_HANDLERS_H
