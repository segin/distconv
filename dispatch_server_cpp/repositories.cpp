#include "repositories.h"
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace distconv {
namespace DispatchServer {

// RAII wrapper for sqlite3_stmt
class StatementFinalizer {
public:
    explicit StatementFinalizer(sqlite3_stmt* stmt) : stmt_(stmt) {}
    ~StatementFinalizer() {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
    }
    // Prevent copying
    StatementFinalizer(const StatementFinalizer&) = delete;
    StatementFinalizer& operator=(const StatementFinalizer&) = delete;
private:
    sqlite3_stmt* stmt_;
};

// SqliteJobRepository implementation
SqliteJobRepository::SqliteJobRepository(const std::string& db_path) : db_path_(db_path), db_(nullptr) {
    initialize_database();
}

SqliteJobRepository::~SqliteJobRepository() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void SqliteJobRepository::initialize_database() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc) {
        std::string msg = sqlite3_errmsg(db_);
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw std::runtime_error("Cannot open database: " + msg);
    }
    
    // Enable WAL mode
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    // Set busy timeout
    sqlite3_busy_timeout(db_, 5000);

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
    rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Cannot create jobs table: " + error);
    }
}

void SqliteJobRepository::execute_sql(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("SQL execution failed: " + error);
    }
}

nlohmann::json SqliteJobRepository::execute_query(const std::string& sql) {
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    StatementFinalizer guard(stmt);
    
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
    
    return result;
}

void SqliteJobRepository::save_job(const std::string& job_id, const nlohmann::json& job) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* sql = R"(
        INSERT OR REPLACE INTO jobs (job_id, job_data, updated_at) 
        VALUES (?, ?, datetime('now'))
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    StatementFinalizer guard(stmt);
    
    std::string job_data = job.dump();
    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, job_data.c_str(), -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Cannot save job: " + std::string(sqlite3_errmsg(db_)));
    }
}

nlohmann::json SqliteJobRepository::get_job_internal(const std::string& job_id) {
    const char* sql = "SELECT job_data FROM jobs WHERE job_id = ?";
    
    sqlite3_finalize(stmt);
}

nlohmann::json SqliteJobRepository::get_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* sql = "SELECT job_data FROM jobs WHERE job_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db_)));
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
    
    return result;
}

std::vector<nlohmann::json> SqliteJobRepository::get_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<nlohmann::json> jobs;
    const std::string sql = "SELECT job_data FROM jobs ORDER BY created_at DESC";
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    StatementGuard guard(stmt);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* job_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (job_data) {
            try {
                jobs.push_back(nlohmann::json::parse(job_data));
            } catch (...) {}
        }
    }
    
    return jobs;
}

bool SqliteJobRepository::job_exists(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return job_exists_internal(job_id);
}

void SqliteJobRepository::remove_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string sql = "DELETE FROM jobs WHERE job_id = ?";
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    StatementGuard guard(stmt);

    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
}

void SqliteJobRepository::clear_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    execute_sql("DELETE FROM jobs");
}

nlohmann::json SqliteJobRepository::get_next_pending_job_by_priority(const std::vector<std::string>& capable_engines) {
    return get_next_pending_job(capable_engines);
}

nlohmann::json SqliteJobRepository::get_next_pending_job(const std::vector<std::string>& capable_engines) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string sql = R"(
        SELECT job_data FROM jobs 
        WHERE status = 'pending'
        ORDER BY 
            priority DESC,
            created_at ASC
        LIMIT 1
    )";
    
    nlohmann::json result = execute_query(sql);
    if (!result.empty()) {
        try {
            return nlohmann::json::parse(result[0]["job_data"].get<std::string>());
        } catch (...) {}
    }
    
    return nullptr;
}

void SqliteJobRepository::mark_job_as_failed_retry(const std::string& job_id, int64_t retry_after_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        nlohmann::json job = get_job_internal(job_id);
        if (!job.is_null()) {
            job["status"] = "failed_retry";
            job["retry_after"] = retry_after_timestamp;
            save_job_internal(job_id, job);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in mark_job_as_failed_retry: " << e.what() << std::endl;
    }
}

std::vector<std::string> SqliteJobRepository::get_stale_pending_jobs(int64_t timeout_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> stale_jobs;
    
    std::string sql = "SELECT job_id FROM jobs WHERE json_extract(job_data, '$.status') = 'pending' AND "
                      "strftime('%s', 'now') - strftime('%s', created_at) > " + std::to_string(timeout_seconds);
                      
    nlohmann::json result = execute_query(sql);
    for (const auto& row : result) {
        if (row.contains("job_id")) {
            stale_jobs.push_back(row["job_id"]);
        }
    }
    return stale_jobs;
}


bool SqliteJobRepository::update_job(const std::string& job_id, const nlohmann::json& updates) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!job_exists_internal(job_id)) {
        return false;
    }
    
    nlohmann::json job = get_job_internal(job_id);
    if (job.is_null()) return false;
    
    // Update only allowed fields
    if (updates.contains("priority")) {
        job["priority"] = updates["priority"];
    }
    if (updates.contains("max_retries")) {
        job["max_retries"] = updates["max_retries"];
    }
    if (updates.contains("resource_requirements")) {
        job["resource_requirements"] = updates["resource_requirements"];
    }
    
    job["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    save_job_internal(job_id, job);
    return true;
}

std::vector<nlohmann::json> SqliteJobRepository::get_jobs_by_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string sql = "SELECT job_data FROM jobs WHERE json_extract(job_data, '$.assigned_engine') = '" + engine_id + "'";
    nlohmann::json result = execute_query(sql);
    
    std::vector<nlohmann::json> jobs;
    for (const auto& row : result) {
        if (row.contains("job_data")) {
            try {
                jobs.push_back(nlohmann::json::parse(row["job_data"].get<std::string>()));
            } catch (...) {
                // Skip invalid JSON
            }
        }
    }
    
    return jobs;
}

bool SqliteJobRepository::update_job_progress(const std::string& job_id, int progress, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!job_exists_internal(job_id)) {
        return false;
    }
    
    nlohmann::json job = get_job_internal(job_id);
    if (job.is_null()) return false;

    job["progress"] = progress;
    job["progress_message"] = message;
    job["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    save_job_internal(job_id, job);
    return true;
}

std::vector<nlohmann::json> SqliteJobRepository::get_timed_out_jobs(int64_t older_than_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Select jobs that are active (assigned/processing) and haven't been updated recently
    std::string sql = "SELECT job_data FROM jobs WHERE "
                      "json_extract(job_data, '$.status') IN ('assigned', 'processing') AND "
                      "json_extract(job_data, '$.updated_at') < " + std::to_string(older_than_timestamp);

    nlohmann::json result = execute_query(sql);
    std::vector<nlohmann::json> jobs;

    for (const auto& row : result) {
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








// SqliteEngineRepository implementation
SqliteEngineRepository::SqliteEngineRepository(const std::string& db_path) : db_path_(db_path), db_(nullptr) {
    initialize_database();
}

SqliteEngineRepository::~SqliteEngineRepository() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void SqliteEngineRepository::initialize_database() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc) {
        std::string msg = sqlite3_errmsg(db_);
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw std::runtime_error("Cannot open database: " + msg);
    }
    
    // Enable WAL mode
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    // Set busy timeout
    sqlite3_busy_timeout(db_, 5000);

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
    rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Cannot create engines table: " + error);
    }
}

void SqliteEngineRepository::execute_sql(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("SQL execution failed: " + error);
    }
}

nlohmann::json SqliteEngineRepository::execute_query(const std::string& sql) {
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    StatementFinalizer guard(stmt);
    
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
    
    return result;
}

void SqliteEngineRepository::save_engine(const std::string& engine_id, const nlohmann::json& engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* sql = R"(
        INSERT OR REPLACE INTO engines (engine_id, engine_data, updated_at) 
        VALUES (?, ?, datetime('now'))
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    StatementFinalizer guard(stmt);
    
    std::string engine_data = engine.dump();
    sqlite3_bind_text(stmt, 1, engine_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, engine_data.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Cannot save engine: " + std::string(sqlite3_errmsg(db_)));
    }
    
    sqlite3_finalize(stmt);
}

nlohmann::json SqliteEngineRepository::get_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* sql = "SELECT engine_data FROM engines WHERE engine_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    StatementFinalizer guard(stmt);
    
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
    
    return result;
}

bool SqliteEngineRepository::engine_exists_internal(const std::string& engine_id) {
    const char* sql = "SELECT 1 FROM engines WHERE engine_id = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    StatementFinalizer guard(stmt);
    
    sqlite3_bind_text(stmt, 1, engine_id.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    return (rc == SQLITE_ROW);
}

// Public Methods

void SqliteEngineRepository::save_engine(const std::string& engine_id, const nlohmann::json& engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    save_engine_internal(engine_id, engine);
}

nlohmann::json SqliteEngineRepository::get_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_engine_internal(engine_id);
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
    return engine_exists_internal(engine_id);
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

nlohmann::json InMemoryJobRepository::get_next_pending_job(const std::vector<std::string>& capable_engines) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json best_job = nullptr;
    int highest_priority = -1;
    int64_t oldest_created_at = std::numeric_limits<int64_t>::max();
    
    for (const auto& [id, job] : jobs_.items()) {
        if (job.value("status", "") == "pending") {
            int priority = job.value("priority", 0);
            int64_t created_at = job.value("created_at", 0);
            
            if (priority > highest_priority) {
                highest_priority = priority;
                oldest_created_at = created_at;
                best_job = job;
            } else if (priority == highest_priority) {
                if (created_at < oldest_created_at) {
                    oldest_created_at = created_at;
                    best_job = job;
                }
            }
        }
    }
    return best_job;
}

void InMemoryJobRepository::mark_job_as_failed_retry(const std::string& job_id, int64_t retry_after_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (jobs_.contains(job_id)) {
        jobs_[job_id]["status"] = "failed_retry";
        jobs_[job_id]["retry_after"] = retry_after_timestamp;
    }
}

std::vector<std::string> InMemoryJobRepository::get_stale_pending_jobs(int64_t timeout_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> stale_jobs;
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    for (const auto& [id, job] : jobs_.items()) {
        if (job.value("status", "") == "pending") {
            int64_t created_at = job.value("created_at", 0);
            if ((now_ms - created_at) / 1000 > timeout_seconds) {
                stale_jobs.push_back(id);
            }
        }
    }
    return stale_jobs;
}


bool InMemoryJobRepository::update_job(const std::string& job_id, const nlohmann::json& updates) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!jobs_.contains(job_id)) {
        return false;
    }
    
    // Update only allowed fields
    if (updates.contains("priority")) {
        jobs_[job_id]["priority"] = updates["priority"];
    }
    if (updates.contains("max_retries")) {
        jobs_[job_id]["max_retries"] = updates["max_retries"];
    }
    if (updates.contains("resource_requirements")) {
        jobs_[job_id]["resource_requirements"] = updates["resource_requirements"];
    }
    
    jobs_[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return true;
}

std::vector<nlohmann::json> InMemoryJobRepository::get_jobs_by_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<nlohmann::json> result;
    for (const auto& [job_id, job] : jobs_.items()) {
        if (job.value("assigned_engine", "") == engine_id) {
            result.push_back(job);
        }
    }
    
    return result;
}

bool InMemoryJobRepository::update_job_progress(const std::string& job_id, int progress, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!jobs_.contains(job_id)) {
        return false;
    }
    
    jobs_[job_id]["progress"] = progress;
    jobs_[job_id]["progress_message"] = message;
    jobs_[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return true;
}

std::vector<nlohmann::json> InMemoryJobRepository::get_timed_out_jobs(int64_t older_than_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<nlohmann::json> result;

    for (const auto& [id, job] : jobs_.items()) {
        std::string status = job.value("status", "");
        if (status == "assigned" || status == "processing") {
            int64_t updated_at = 0;
            if (job.contains("updated_at")) {
                updated_at = job["updated_at"];
            }

            if (updated_at < older_than_timestamp) {
                result.push_back(job);
            }
        }
    }
    return result;
}

nlohmann::json InMemoryJobRepository::get_next_pending_job_by_priority(const std::vector<std::string>& capable_engines) {
    // For now, just reuse get_next_pending_job which already sorts by priority
    return get_next_pending_job(capable_engines);
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

} // namespace DispatchServer
} // namespace distconv
