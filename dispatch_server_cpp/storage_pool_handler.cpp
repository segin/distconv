#include "storage_pool_handler.h"
#include "dispatch_server_core.h"
#include "dispatch_server_constants.h"
#include <uuid/uuid.h>

namespace distconv {
namespace DispatchServer {

using namespace Constants;

// InMemoryStorageRepository implementation
void InMemoryStorageRepository::save_pool(const std::string& pool_id, const nlohmann::json& pool) {
    std::lock_guard<std::mutex> lock(mutex_);
    pools_[pool_id] = pool;
}

nlohmann::json InMemoryStorageRepository::get_pool(const std::string& pool_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pools_.contains(pool_id)) {
        return pools_[pool_id];
    }
    return nlohmann::json();
}

std::vector<nlohmann::json> InMemoryStorageRepository::get_all_pools() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<nlohmann::json> result;
    for (const auto& [pool_id, pool] : pools_.items()) {
        result.push_back(pool);
    }
    return result;
}

bool InMemoryStorageRepository::pool_exists(const std::string& pool_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return pools_.contains(pool_id);
}

void InMemoryStorageRepository::remove_pool(const std::string& pool_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    pools_.erase(pool_id);
}

// StoragePoolCreateHandler implementation
StoragePoolCreateHandler::StoragePoolCreateHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IStorageRepository> storage_repo)
    : auth_(auth), storage_repo_(storage_repo) {}

void StoragePoolCreateHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // Parse request body
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (...) {
        set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400);
        return;
    }

    // Validate required fields
    if (!request_json.contains("name") || !request_json["name"].is_string()) {
        set_json_error_response(res, "Missing or invalid 'name' field", "validation_error", 400);
        return;
    }

    if (!request_json.contains("capacity_gb") || !request_json["capacity_gb"].is_number()) {
        set_json_error_response(res, "Missing or invalid 'capacity_gb' field", "validation_error", 400);
        return;
    }

    // Generate pool ID
    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    std::string pool_id = std::string(uuid_str);

    // Create pool object
    nlohmann::json pool;
    pool["id"] = pool_id;
    pool["name"] = request_json["name"];
    pool["capacity_gb"] = request_json["capacity_gb"];
    pool["used_gb"] = request_json.value("used_gb", 0);
    pool["path"] = request_json.value("path", "");
    pool["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    storage_repo_->save_pool(pool_id, pool);
    set_json_response(res, pool, 201);
}

// StoragePoolListHandler implementation
StoragePoolListHandler::StoragePoolListHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IStorageRepository> storage_repo)
    : auth_(auth), storage_repo_(storage_repo) {}

void StoragePoolListHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    std::vector<nlohmann::json> pools = storage_repo_->get_all_pools();
    nlohmann::json response = nlohmann::json::array();
    for (const auto& pool : pools) {
        response.push_back(pool);
    }

    set_json_response(res, response, 200);
}

// StoragePoolUpdateHandler implementation
StoragePoolUpdateHandler::StoragePoolUpdateHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IStorageRepository> storage_repo)
    : auth_(auth), storage_repo_(storage_repo) {}

void StoragePoolUpdateHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // Extract pool ID from URL
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Invalid or missing pool ID in URL", "validation_error", 400);
        return;
    }
    std::string pool_id = req.matches[1];

    // Check if pool exists
    if (!storage_repo_->pool_exists(pool_id)) {
        set_json_error_response(res, "Storage pool not found", "not_found", 404, "Pool ID: " + pool_id);
        return;
    }

    // Parse request body
    nlohmann::json request_json;
    try {
        request_json = nlohmann::json::parse(req.body);
    } catch (...) {
        set_json_error_response(res, "Invalid JSON in request body", "json_parse_error", 400);
        return;
    }

    // Get existing pool and update allowed fields
    nlohmann::json pool = storage_repo_->get_pool(pool_id);
    
    if (request_json.contains("name")) {
        pool["name"] = request_json["name"];
    }
    if (request_json.contains("capacity_gb")) {
        pool["capacity_gb"] = request_json["capacity_gb"];
    }
    if (request_json.contains("used_gb")) {
        pool["used_gb"] = request_json["used_gb"];
    }
    if (request_json.contains("path")) {
        pool["path"] = request_json["path"];
    }

    pool["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    storage_repo_->save_pool(pool_id, pool);
    set_json_response(res, pool, 200);
}

// StoragePoolDeleteHandler implementation
StoragePoolDeleteHandler::StoragePoolDeleteHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IStorageRepository> storage_repo)
    : auth_(auth), storage_repo_(storage_repo) {}

void StoragePoolDeleteHandler::handle(const httplib::Request& req, httplib::Response& res) {
    // Authentication
    if (!auth_->authenticate(req, res)) {
        return;
    }

    // Extract pool ID from URL
    if (req.matches.size() < 2) {
        set_json_error_response(res, "Invalid or missing pool ID in URL", "validation_error", 400);
        return;
    }
    std::string pool_id = req.matches[1];

    // Check if pool exists
    if (!storage_repo_->pool_exists(pool_id)) {
        set_json_error_response(res, "Storage pool not found", "not_found", 404, "Pool ID: " + pool_id);
        return;
    }

    storage_repo_->remove_pool(pool_id);
    
    nlohmann::json response;
    response["message"] = "Storage pool deleted successfully";
    response["pool_id"] = pool_id;
    set_json_response(res, response, 200);
}

} // namespace DispatchServer
} // namespace distconv
