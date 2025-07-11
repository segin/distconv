#ifndef REPOSITORIES_H
#define REPOSITORIES_H

#include "nlohmann/json.hpp"
#include <string>
#include <vector>
#include <memory>
#include <mutex>

// Abstract interface for job repository
class IJobRepository {
public:
    virtual ~IJobRepository() = default;
    virtual void save_job(const std::string& job_id, const nlohmann::json& job) = 0;
    virtual nlohmann::json get_job(const std::string& job_id) = 0;
    virtual std::vector<nlohmann::json> get_all_jobs() = 0;
    virtual bool job_exists(const std::string& job_id) = 0;
    virtual void remove_job(const std::string& job_id) = 0;
    virtual void clear_all_jobs() = 0;
};

// Abstract interface for engine repository
class IEngineRepository {
public:
    virtual ~IEngineRepository() = default;
    virtual void save_engine(const std::string& engine_id, const nlohmann::json& engine) = 0;
    virtual nlohmann::json get_engine(const std::string& engine_id) = 0;
    virtual std::vector<nlohmann::json> get_all_engines() = 0;
    virtual bool engine_exists(const std::string& engine_id) = 0;
    virtual void remove_engine(const std::string& engine_id) = 0;
    virtual void clear_all_engines() = 0;
};

// SQLite-based job repository implementation
class SqliteJobRepository : public IJobRepository {
private:
    std::string db_path_;
    mutable std::mutex mutex_;
    
    void initialize_database();
    void execute_sql(const std::string& sql);
    nlohmann::json execute_query(const std::string& sql);
    
public:
    explicit SqliteJobRepository(const std::string& db_path);
    ~SqliteJobRepository();
    
    void save_job(const std::string& job_id, const nlohmann::json& job) override;
    nlohmann::json get_job(const std::string& job_id) override;
    std::vector<nlohmann::json> get_all_jobs() override;
    bool job_exists(const std::string& job_id) override;
    void remove_job(const std::string& job_id) override;
    void clear_all_jobs() override;
};

// SQLite-based engine repository implementation
class SqliteEngineRepository : public IEngineRepository {
private:
    std::string db_path_;
    mutable std::mutex mutex_;
    
    void initialize_database();
    void execute_sql(const std::string& sql);
    nlohmann::json execute_query(const std::string& sql);
    
public:
    explicit SqliteEngineRepository(const std::string& db_path);
    ~SqliteEngineRepository();
    
    void save_engine(const std::string& engine_id, const nlohmann::json& engine) override;
    nlohmann::json get_engine(const std::string& engine_id) override;
    std::vector<nlohmann::json> get_all_engines() override;
    bool engine_exists(const std::string& engine_id) override;
    void remove_engine(const std::string& engine_id) override;
    void clear_all_engines() override;
};

// In-memory implementations for testing
class InMemoryJobRepository : public IJobRepository {
private:
    nlohmann::json jobs_;
    mutable std::mutex mutex_;
    
public:
    void save_job(const std::string& job_id, const nlohmann::json& job) override;
    nlohmann::json get_job(const std::string& job_id) override;
    std::vector<nlohmann::json> get_all_jobs() override;
    bool job_exists(const std::string& job_id) override;
    void remove_job(const std::string& job_id) override;
    void clear_all_jobs() override;
};

class InMemoryEngineRepository : public IEngineRepository {
private:
    nlohmann::json engines_;
    mutable std::mutex mutex_;
    
public:
    void save_engine(const std::string& engine_id, const nlohmann::json& engine) override;
    nlohmann::json get_engine(const std::string& engine_id) override;
    std::vector<nlohmann::json> get_all_engines() override;
    bool engine_exists(const std::string& engine_id) override;
    void remove_engine(const std::string& engine_id) override;
    void clear_all_engines() override;
};

#endif // REPOSITORIES_H