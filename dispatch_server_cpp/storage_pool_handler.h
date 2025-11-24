#ifndef STORAGE_POOL_HANDLER_H
#define STORAGE_POOL_HANDLER_H

#include "request_handlers.h"
#include "nlohmann/json.hpp"
#include <memory>
#include <mutex>
#include <map>

namespace distconv {
namespace DispatchServer {

// Simple in-memory storage repository for storage pools
class IStorageRepository {
public:
    virtual ~IStorageRepository() = default;
    virtual void save_pool(const std::string& pool_id, const nlohmann::json& pool) = 0;
    virtual nlohmann::json get_pool(const std::string& pool_id) = 0;
    virtual std::vector<nlohmann::json> get_all_pools() = 0;
    virtual bool pool_exists(const std::string& pool_id) = 0;
    virtual void remove_pool(const std::string& pool_id) = 0;
};

class InMemoryStorageRepository : public IStorageRepository {
private:
    nlohmann::json pools_;
    mutable std::mutex mutex_;

public:
    void save_pool(const std::string& pool_id, const nlohmann::json& pool) override;
    nlohmann::json get_pool(const std::string& pool_id) override;
    std::vector<nlohmann::json> get_all_pools() override;
    bool pool_exists(const std::string& pool_id) override;
    void remove_pool(const std::string& pool_id) override;
};

// Handler for POST /storage_pools/ - Create storage pool
class StoragePoolCreateHandler : public IRequestHandler {
public:
    explicit StoragePoolCreateHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IStorageRepository> storage_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;

private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IStorageRepository> storage_repo_;
};

// Handler for GET /storage_pools/ - List all storage pools
class StoragePoolListHandler : public IRequestHandler {
public:
    explicit StoragePoolListHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IStorageRepository> storage_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;

private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IStorageRepository> storage_repo_;
};

// Handler for PUT /storage_pools/{id} - Update storage pool
class StoragePoolUpdateHandler : public IRequestHandler {
public:
    explicit StoragePoolUpdateHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IStorageRepository> storage_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;

private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IStorageRepository> storage_repo_;
};

// Handler for DELETE /storage_pools/{id} - Delete storage pool
class StoragePoolDeleteHandler : public IRequestHandler {
public:
    explicit StoragePoolDeleteHandler(std::shared_ptr<AuthMiddleware> auth, std::shared_ptr<IStorageRepository> storage_repo);
    void handle(const httplib::Request& req, httplib::Response& res) override;

private:
    std::shared_ptr<AuthMiddleware> auth_;
    std::shared_ptr<IStorageRepository> storage_repo_;
};

} // namespace DispatchServer
} // namespace distconv

#endif  // STORAGE_POOL_HANDLER_H
