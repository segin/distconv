#include "repositories.h"
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

// SqliteJobRepository implementation
SqliteJobRepository::SqliteJobRepository(const std::string& db_path) : db_path_(db_path) {
    initialize_database();
}

SqliteJobRepository::~SqliteJobRepository() {
    // SQLite cleanup handled by sqlite3_close
}

void SqliteJobRepository::initialize_database() {
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS jobs (
            job_id TEXT PRIMARY KEY,
            job_data TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE INDEX IF NOT EXISTS idx_jobs_created_at ON jobs(created_at);
    )";
    
    char* err_msg = nullptr;
    rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        sqlite3_close(db);
        throw std::runtime_error("Cannot create jobs table: " + error);
    }
    
    sqlite3_close(db);
}

void SqliteJobRepository::execute_sql(const std::string& sql) {
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    
    char* err_msg = nullptr;
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        sqlite3_close(db);
        throw std::runtime_error("SQL execution failed: " + error);
    }
    
    sqlite3_close(db);
}

nlohmann::json SqliteJobRepository::execute_query(const std::string& sql) {
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    nlohmann::json result = nlohmann::json::array();
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        nlohmann::json row;
        int cols = sqlite3_column_count(stmt);
        for (int i = 0; i < cols; i++) {
            const char* col_name = sqlite3_column_name(stmt, i);
            const char* col_value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row[col_name] = col_value ? col_value : "";
        }
        result.push_back(row);
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return result;
}

void SqliteJobRepository::save_job(const std::string& job_id, const nlohmann::json& job) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    
    const char* sql = R"(
        INSERT OR REPLACE INTO jobs (job_id, job_data, updated_at) 
        VALUES (?, ?, datetime('now'))
    )";
    
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    std::string job_data = job.dump();
    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, job_data.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        throw std::runtime_error("Cannot save job: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

nlohmann::json SqliteJobRepository::get_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    
    const char* sql = "SELECT job_data FROM jobs WHERE job_id = ?";
    
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_STATIC);
    
    nlohmann::json result;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* job_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (job_data) {
            result = nlohmann::json::parse(job_data);
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return result;
}

std::vector<nlohmann::json> SqliteJobRepository::get_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json query_result = execute_query("SELECT job_data FROM jobs ORDER BY created_at DESC");
    std::vector<nlohmann::json> jobs;
    
    for (const auto& row : query_result) {
        if (row.contains("job_data")) {
            try {
                jobs.push_back(nlohmann::json::parse(row["job_data"].get<std::string>()));
            } catch (const std::exception&) {
                // Skip invalid JSON
            }
        }
    }
    
    return jobs;
}

bool SqliteJobRepository::job_exists(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json result = execute_query("SELECT COUNT(*) as count FROM jobs WHERE job_id = '" + job_id + "'");
    if (!result.empty() && result[0].contains("count")) {
        return std::stoi(result[0]["count"].get<std::string>()) > 0;
    }
    return false;
}

void SqliteJobRepository::remove_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    execute_sql("DELETE FROM jobs WHERE job_id = '" + job_id + "'");
}

void SqliteJobRepository::clear_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    execute_sql("DELETE FROM jobs");
}

// SqliteEngineRepository implementation
SqliteEngineRepository::SqliteEngineRepository(const std::string& db_path) : db_path_(db_path) {
    initialize_database();
}

SqliteEngineRepository::~SqliteEngineRepository() {
    // SQLite cleanup handled by sqlite3_close
}

void SqliteEngineRepository::initialize_database() {
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS engines (
            engine_id TEXT PRIMARY KEY,
            engine_data TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE INDEX IF NOT EXISTS idx_engines_updated_at ON engines(updated_at);
    )";
    
    char* err_msg = nullptr;
    rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        sqlite3_close(db);
        throw std::runtime_error("Cannot create engines table: " + error);
    }
    
    sqlite3_close(db);
}

void SqliteEngineRepository::execute_sql(const std::string& sql) {
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    
    char* err_msg = nullptr;
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        sqlite3_close(db);
        throw std::runtime_error("SQL execution failed: " + error);
    }
    
    sqlite3_close(db);
}

nlohmann::json SqliteEngineRepository::execute_query(const std::string& sql) {
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    nlohmann::json result = nlohmann::json::array();
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        nlohmann::json row;
        int cols = sqlite3_column_count(stmt);
        for (int i = 0; i < cols; i++) {
            const char* col_name = sqlite3_column_name(stmt, i);
            const char* col_value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row[col_name] = col_value ? col_value : "";
        }
        result.push_back(row);
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return result;
}

void SqliteEngineRepository::save_engine(const std::string& engine_id, const nlohmann::json& engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    
    const char* sql = R"(
        INSERT OR REPLACE INTO engines (engine_id, engine_data, updated_at) 
        VALUES (?, ?, datetime('now'))
    )";
    
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    std::string engine_data = engine.dump();
    sqlite3_bind_text(stmt, 1, engine_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, engine_data.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        throw std::runtime_error("Cannot save engine: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

nlohmann::json SqliteEngineRepository::get_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    }
    
    const char* sql = "SELECT engine_data FROM engines WHERE engine_id = ?";
    
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, engine_id.c_str(), -1, SQLITE_STATIC);
    
    nlohmann::json result;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* engine_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (engine_data) {
            result = nlohmann::json::parse(engine_data);
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return result;
}

std::vector<nlohmann::json> SqliteEngineRepository::get_all_engines() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json query_result = execute_query("SELECT engine_data FROM engines ORDER BY updated_at DESC");
    std::vector<nlohmann::json> engines;
    
    for (const auto& row : query_result) {
        if (row.contains("engine_data")) {
            try {
                engines.push_back(nlohmann::json::parse(row["engine_data"].get<std::string>()));
            } catch (const std::exception&) {
                // Skip invalid JSON
            }
        }
    }
    
    return engines;
}

bool SqliteEngineRepository::engine_exists(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json result = execute_query("SELECT COUNT(*) as count FROM engines WHERE engine_id = '" + engine_id + "'");
    if (!result.empty() && result[0].contains("count")) {
        return std::stoi(result[0]["count"].get<std::string>()) > 0;
    }
    return false;
}

void SqliteEngineRepository::remove_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    execute_sql("DELETE FROM engines WHERE engine_id = '" + engine_id + "'");
}

void SqliteEngineRepository::clear_all_engines() {
    std::lock_guard<std::mutex> lock(mutex_);
    execute_sql("DELETE FROM engines");
}

// InMemoryJobRepository implementation
void InMemoryJobRepository::save_job(const std::string& job_id, const nlohmann::json& job) {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_[job_id] = job;
}

nlohmann::json InMemoryJobRepository::get_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (jobs_.contains(job_id)) {
        return jobs_[job_id];
    }
    return nlohmann::json();
}

std::vector<nlohmann::json> InMemoryJobRepository::get_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<nlohmann::json> result;
    for (const auto& [key, job] : jobs_.items()) {
        result.push_back(job);
    }
    return result;
}

bool InMemoryJobRepository::job_exists(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return jobs_.contains(job_id);
}

void InMemoryJobRepository::remove_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.erase(job_id);
}

void InMemoryJobRepository::clear_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.clear();
}

// InMemoryEngineRepository implementation
void InMemoryEngineRepository::save_engine(const std::string& engine_id, const nlohmann::json& engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    engines_[engine_id] = engine;
}

nlohmann::json InMemoryEngineRepository::get_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (engines_.contains(engine_id)) {
        return engines_[engine_id];
    }
    return nlohmann::json();
}

std::vector<nlohmann::json> InMemoryEngineRepository::get_all_engines() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<nlohmann::json> result;
    for (const auto& [key, engine] : engines_.items()) {
        result.push_back(engine);
    }
    return result;
}

bool InMemoryEngineRepository::engine_exists(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return engines_.contains(engine_id);
}

void InMemoryEngineRepository::remove_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    engines_.erase(engine_id);
}

void InMemoryEngineRepository::clear_all_engines() {
    std::lock_guard<std::mutex> lock(mutex_);
    engines_.clear();
}