#include "engine_handlers.h"
#include "dispatch_server_core.h"
#include <mutex>

namespace distconv {
namespace DispatchServer {

// ==================== EngineListHandler ====================

EngineListHandler::EngineListHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IEngineRepository> engine_repo)
    : auth_(auth), engine_repo_(engine_repo) {}

void EngineListHandler::handle(const httplib::Request& req, httplib::Response& res) {
    if (!auth_->authenticate(req, res)) return;
    auto all_engines_vec = engine_repo_->get_all_engines();
    nlohmann::json all_engines = all_engines_vec;
    set_json_response(res, all_engines, 200);
}

// ==================== EngineHeartbeatHandler ====================

EngineHeartbeatHandler::EngineHeartbeatHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IEngineRepository> engine_repo)
    : auth_(auth), engine_repo_(engine_repo) {}

bool EngineHeartbeatHandler::validate_heartbeat_input(const nlohmann::json& input, httplib::Response& res) {
    if (!input.contains("engine_id") || !input["engine_id"].is_string()) {
        set_json_error_response(res, "Bad Request: 'engine_id' is missing or not a string.", "validation_error", 400);
        return false;
    }
    return true;
}

void EngineHeartbeatHandler::handle(const httplib::Request& req, httplib::Response& res) {
    if (!auth_->authenticate(req, res)) return;
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400, e.what());
        return;
    }

    if (!validate_heartbeat_input(request_json, res)) return;

    std::string engine_id = request_json["engine_id"];
    try {
        request_json["last_heartbeat"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        engine_repo_->save_engine(engine_id, request_json);
        res.set_content("Heartbeat received from engine " + engine_id, "text/plain");
    } catch (const std::exception& e) {
        set_json_error_response(res, "Internal server error", "server_error", 500, e.what());
    }
}

// ==================== EngineBenchmarkHandler ====================

EngineBenchmarkHandler::EngineBenchmarkHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IEngineRepository> engine_repo)
    : auth_(auth), engine_repo_(engine_repo) {}

void EngineBenchmarkHandler::handle(const httplib::Request& req, httplib::Response& res) {
    if (!auth_->authenticate(req, res)) return;
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400, e.what());
        return;
    }

    if (!request_json.contains("engine_id")) {
        set_json_error_response(res, "Bad Request: 'engine_id' is missing.", "validation_error", 400);
        return;
    }
    std::string engine_id = request_json["engine_id"];

    try {
        nlohmann::json engine = engine_repo_->get_engine(engine_id);
        if (engine.is_null() || engine.empty()) {
            res.status = 404;
            res.set_content("Engine not found", "text/plain");
            return;
        }
        engine["benchmark_time"] = request_json.value("benchmark_time", 0.0);
        engine_repo_->save_engine(engine_id, engine);
        res.set_content("Benchmark result received from engine " + engine_id, "text/plain");
    } catch (const std::exception& e) {
        set_json_error_response(res, "Internal server error", "server_error", 500, e.what());
    }
}

} // namespace DispatchServer
} // namespace distconv
