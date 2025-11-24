#include "engine_handlers.h"
#include "dispatch_server_core.h"
#include <mutex>

// ==================== EngineListHandler ====================

EngineListHandler::EngineListHandler(std::shared_ptr<AuthMiddleware> auth)
    : auth_(auth) {}

void EngineListHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // 1. Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // 2. Retrieve All Engines
    nlohmann::json all_engines = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (!engines_db.empty()) {
            for (auto const& [key, val] : engines_db.items()) {
                all_engines.push_back(val);
            }
        }
    }

    // 3. Return Response
    set_json_response(res, all_engines, 200);
}

// ==================== EngineHeartbeatHandler ====================

EngineHeartbeatHandler::EngineHeartbeatHandler(std::shared_ptr<AuthMiddleware> auth)
    : auth_(auth) {}

bool EngineHeartbeatHandler::validate_heartbeat_input(const nlohmann::json& input, httplib::Response& res) {
    // Validate engine_id
    if (!input.contains("engine_id")) {
        set_error_response(res, "Bad Request: 'engine_id' is missing.", 400);
        return false;
    }
    if (!input["engine_id"].is_string()) {
        set_error_response(res, "Bad Request: 'engine_id' must be a string.", 400);
        return false;
    }

    // Validate storage_capacity_gb if present
    if (input.contains("storage_capacity_gb")) {
        if (!input["storage_capacity_gb"].is_number()) {
            set_error_response(res, "Bad Request: 'storage_capacity_gb' must be a number.", 400);
            return false;
        }
        if (input["storage_capacity_gb"].get<double>() < 0) {
            set_error_response(res, "Bad Request: 'storage_capacity_gb' must be a non-negative number.", 400);
            return false;
        }
    }

    // Validate streaming_support if present
    if (input.contains("streaming_support") && !input["streaming_support"].is_boolean()) {
        set_error_response(res, "Bad Request: 'streaming_support' must be a boolean.", 400);
        return false;
    }

    return true;
}

void EngineHeartbeatHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // 1. Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // 2. Parse JSON
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_error_response(res, "Invalid JSON: " + std::string(e.what()), 400);
        return;
    }

    // 3. Validate Input
    if (!validate_heartbeat_input(request_json, res)) {
        return;
    }

    // 4. Update Engine Database
    std::string engine_id = request_json["engine_id"];
    try {
        std::lock_guard<std::mutex> lock(state_mutex);
        engines_db[engine_id] = request_json;
        save_state_unlocked();
    } catch (const std::exception& e) {
        set_error_response(res, "Server error: " + std::string(e.what()), 500);
        return;
    }

    // 5. Return Success
    res.set_content("Heartbeat received from engine " + engine_id, "text/plain");
}

// ==================== EngineBenchmarkHandler ====================

EngineBenchmarkHandler::EngineBenchmarkHandler(std::shared_ptr<AuthMiddleware> auth)
    : auth_(auth) {}

void EngineBenchmarkHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // 1. Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // 2. Parse JSON
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        set_error_response(res, "Invalid JSON: " + std::string(e.what()), 400);
        return;
    }

    // 3. Validate engine_id
    if (!request_json.contains("engine_id")) {
        set_error_response(res, "Bad Request: 'engine_id' is missing.", 400);
        return;
    }
    std::string engine_id = request_json["engine_id"];

    // 4. Validate benchmark_time if present
    if (request_json.contains("benchmark_time")) {
        if (!request_json["benchmark_time"].is_number()) {
            set_error_response(res, "Bad Request: 'benchmark_time' must be a number.", 400);
            return;
        }
        if (request_json["benchmark_time"].get<double>() < 0) {
            set_error_response(res, "Bad Request: 'benchmark_time' must be a non-negative number.", 400);
            return;
        }
    }

    // 5. Update Engine Benchmark Data
    try {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (engines_db.contains(engine_id)) {
            engines_db[engine_id]["benchmark_time"] = request_json["benchmark_time"];
        } else {
            res.status = 404;
            res.set_content("Engine not found", "text/plain");
            return;
        }
    } catch (const std::exception& e) {
        set_error_response(res, "Server error: " + std::string(e.what()), 500);
        return;
    }

    // 6. Return Success
    res.set_content("Benchmark result received from engine " + engine_id, "text/plain");
}
