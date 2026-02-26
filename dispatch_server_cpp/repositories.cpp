#include "repositories.h"
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <chrono>

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
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto const& [sql, stmt] : statements_) {
        sqlite3_finalize(stmt);
    }
    statements_.clear();
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
            status TEXT NOT NULL,
            priority INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE INDEX IF NOT EXISTS idx_jobs_status ON jobs(status);
        CREATE INDEX IF NOT EXISTS idx_jobs_priority ON jobs(priority);
        CREATE INDEX IF NOT EXISTS idx_jobs_created_at ON jobs(created_at);
        CREATE INDEX IF NOT EXISTS idx_jobs_updated_at ON jobs(updated_at);
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

sqlite3_stmt* SqliteJobRepository::get_prepared_statement(const std::string& sql) {
    if (statements_.find(sql) != statements_.end()) {
        sqlite3_reset(statements_[sql]);
        sqlite3_clear_bindings(statements_[sql]);
        return statements_[sql];
    }
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Cannot prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    statements_[sql] = stmt;
    return stmt;
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
            int col_type = sqlite3_column_type(stmt, i);
            
            if (col_type == SQLITE_NULL) {
                row[col_name] = nullptr;
            } else if (col_type == SQLITE_INTEGER) {
                row[col_name] = sqlite3_column_int64(stmt, i);
            } else if (col_type == SQLITE_FLOAT) {
                row[col_name] = sqlite3_column_double(stmt, i);
            } else {
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                row[col_name] = text ? text : "";
            }
        }
        result.push_back(row);
    }
    
    return result;
}

void SqliteJobRepository::save_job_internal(const std::string& job_id, const nlohmann::json& job) {
    const char* sql = R"(
        INSERT OR REPLACE INTO jobs (job_id, job_data, status, priority, updated_at) 
        VALUES (?, ?, ?, ?, datetime('now'))
    )";
    
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    
    std::string job_data = job.dump();
    std::string status = job.value("status", "pending");
    int priority = job.value("priority", 0);
    
    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, job_data.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, priority);
    
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Cannot save job: " + std::string(sqlite3_errmsg(db_)));
    }
}

void SqliteJobRepository::save_job(const std::string& job_id, const nlohmann::json& job) {
    std::lock_guard<std::mutex> lock(mutex_);
    save_job_internal(job_id, job);
}

nlohmann::json SqliteJobRepository::get_job_internal(const std::string& job_id) {
    const char* sql = "SELECT job_data FROM jobs WHERE job_id = ?";
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    
    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_STATIC);
    
    nlohmann::json result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* job_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (job_data) {
            try {
                result = nlohmann::json::parse(job_data);
            } catch (...) {
                // Return empty if parse fails
            }
        }
    }
    return result;
}

nlohmann::json SqliteJobRepository::get_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_job_internal(job_id);
}

std::vector<nlohmann::json> SqliteJobRepository::get_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<nlohmann::json> jobs;
    const std::string sql = "SELECT job_data FROM jobs ORDER BY created_at DESC";
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    
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

bool SqliteJobRepository::job_exists_internal(const std::string& job_id) {
    const char* sql = "SELECT 1 FROM jobs WHERE job_id = ?";
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_STATIC);
    return (sqlite3_step(stmt) == SQLITE_ROW);
}

bool SqliteJobRepository::job_exists(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return job_exists_internal(job_id);
}

void SqliteJobRepository::remove_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string sql = "DELETE FROM jobs WHERE job_id = ?";
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
}

void SqliteJobRepository::clear_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    execute_sql("DELETE FROM jobs");
}

nlohmann::json SqliteJobRepository::get_next_pending_job(const std::vector<std::string>& capable_engines) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // For now, simple scheduling ignoring capable_engines filter in SQL 
    // unless it becomes a bottleneck. We filter Engines in the scheduler usually.
    // However, we can at least filter by status and order.
    
    const char* sql = R"(
        SELECT job_data FROM jobs 
        WHERE status = 'pending'
        ORDER BY 
            priority DESC,
            created_at ASC
        LIMIT 1
    )";
    
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* job_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (job_data) {
            try {
                return nlohmann::json::parse(job_data);
            } catch (...) {}
        }
    }
    
    return nullptr;
}

nlohmann::json SqliteJobRepository::get_next_pending_job_by_priority(const std::vector<std::string>& capable_engines) {
    return get_next_pending_job(capable_engines);
}

void SqliteJobRepository::mark_job_as_failed_retry(const std::string& job_id, int64_t retry_after_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json job = get_job_internal(job_id);
    if (!job.is_null()) {
        job["status"] = "failed_retry";
        job["retry_after"] = retry_after_timestamp;
        save_job_internal(job_id, job);
    }
}

std::vector<std::string> SqliteJobRepository::get_stale_pending_jobs(int64_t timeout_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> stale_jobs;
    
    std::string sql = "SELECT job_id FROM jobs WHERE status = 'pending' AND "
                      "strftime('%s', 'now') - strftime('%s', created_at) > " + std::to_string(timeout_seconds);
                      
    nlohmann::json result = execute_query(sql);
    for (const auto& row : result) {
        if (row.contains("job_id")) {
            stale_jobs.push_back(row["job_id"].get<std::string>());
        }
    }
    return stale_jobs;
}

std::vector<nlohmann::json> SqliteJobRepository::get_jobs_to_timeout(int timeout_minutes) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string sql = "SELECT job_data FROM jobs WHERE (status = 'assigned' OR status = 'processing') AND "
                      "updated_at < datetime('now', '-" + std::to_string(timeout_minutes) + " minutes')";

    nlohmann::json result = execute_query(sql);
    std::vector<nlohmann::json> jobs;
    for (const auto& row : result) {
        if (row.contains("job_data")) {
            try {
                jobs.push_back(nlohmann::json::parse(row["job_data"].get<std::string>()));
            } catch (...) {}
        }
    }
    return jobs;
}

bool SqliteJobRepository::update_job(const std::string& job_id, const nlohmann::json& updates) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json job = get_job_internal(job_id);
    if (job.is_null()) return false;
    
    // Update fields
    for (auto it = updates.begin(); it != updates.end(); ++it) {
        job[it.key()] = it.value();
    }
    
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    job["updated_at"] = now_ms;
    
    save_job_internal(job_id, job);
    return true;
}

std::vector<nlohmann::json> SqliteJobRepository::get_jobs_by_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<nlohmann::json> jobs;
    const char* sql = "SELECT job_data FROM jobs WHERE json_extract(job_data, '$.assigned_engine') = ?";
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    sqlite3_bind_text(stmt, 1, engine_id.c_str(), -1, SQLITE_STATIC);
    
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

bool SqliteJobRepository::update_job_progress(const std::string& job_id, int progress, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json job = get_job_internal(job_id);
    if (job.is_null()) return false;

    job["progress"] = progress;
    job["progress_message"] = message;
    
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    job["updated_at"] = now_ms;
    
    save_job_internal(job_id, job);
    return true;
}

std::vector<nlohmann::json> SqliteJobRepository::get_jobs_by_status(const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<nlohmann::json> jobs;
    const char* sql = "SELECT job_data FROM jobs WHERE status = ? ORDER BY created_at ASC";
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_STATIC);
    
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

std::vector<nlohmann::json> SqliteJobRepository::get_timed_out_jobs(int64_t older_than_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<nlohmann::json> jobs;
    const char* sql = "SELECT job_data FROM jobs WHERE status IN ('assigned', 'processing')";
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* job_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (job_data) {
            try {
                nlohmann::json job = nlohmann::json::parse(job_data);
                if (job.value("updated_at", 0LL) < older_than_timestamp) {
                    jobs.push_back(job);
                }
            } catch (...) {}
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
    
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_busy_timeout(db_, 5000);

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS engines (
            engine_id TEXT PRIMARY KEY,
            engine_data TEXT NOT NULL,
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
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row[col_name] = text ? text : "";
        }
        result.push_back(row);
    }
    return result;
}

void SqliteEngineRepository::save_engine_internal(const std::string& engine_id, const nlohmann::json& engine) {
    const char* sql = "INSERT OR REPLACE INTO engines (engine_id, engine_data, updated_at) VALUES (?, ?, datetime('now'))";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    StatementFinalizer guard(stmt);
    
    std::string engine_data = engine.dump();
    sqlite3_bind_text(stmt, 1, engine_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, engine_data.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("Cannot save engine: " + std::string(sqlite3_errmsg(db_)));
    }
}

void SqliteEngineRepository::save_engine(const std::string& engine_id, const nlohmann::json& engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    save_engine_internal(engine_id, engine);
}

nlohmann::json SqliteEngineRepository::get_engine_internal(const std::string& engine_id) {
    const char* sql = "SELECT engine_data FROM engines WHERE engine_id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    StatementFinalizer guard(stmt);
    
    sqlite3_bind_text(stmt, 1, engine_id.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* engine_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (engine_data) return nlohmann::json::parse(engine_data);
    }
    return nlohmann::json();
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
        try {
            engines.push_back(nlohmann::json::parse(row["engine_data"].get<std::string>()));
        } catch (...) {}
    }
    return engines;
}

bool SqliteEngineRepository::engine_exists(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "SELECT 1 FROM engines WHERE engine_id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    StatementFinalizer guard(stmt);
    sqlite3_bind_text(stmt, 1, engine_id.c_str(), -1, SQLITE_STATIC);
    return (sqlite3_step(stmt) == SQLITE_ROW);
}

void SqliteEngineRepository::remove_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "DELETE FROM engines WHERE engine_id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    StatementFinalizer guard(stmt);
    sqlite3_bind_text(stmt, 1, engine_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
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
    return jobs_.value(job_id, nlohmann::json());
}

std::vector<nlohmann::json> InMemoryJobRepository::get_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<nlohmann::json> result;
    for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
        result.push_back(it.value());
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
    jobs_ = nlohmann::json::object();
}

nlohmann::json InMemoryJobRepository::get_next_pending_job(const std::vector<std::string>& capable_engines) {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json best_job = nullptr;
    int highest_priority = -1;
    int64_t oldest_created_at = std::numeric_limits<int64_t>::max();
    
    for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
        const auto& job = it.value();
        if (job.value("status", "") == "pending") {
            int priority = job.value("priority", 0);
            int64_t created_at = job.value("created_at", 0LL);
            
            if (priority > highest_priority || (priority == highest_priority && created_at < oldest_created_at)) {
                highest_priority = priority;
                oldest_created_at = created_at;
                best_job = job;
            }
        }
    }
    return best_job;
}

nlohmann::json InMemoryJobRepository::get_next_pending_job_by_priority(const std::vector<std::string>& capable_engines) {
    return get_next_pending_job(capable_engines);
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
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    
    for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
        const auto& job = it.value();
        if (job.value("status", "") == "pending") {
            int64_t created_at = job.value("created_at", 0LL);
            if ((now_ms - created_at) / 1000 > timeout_seconds) {
                stale_jobs.push_back(it.key());
            }
        }
    }
    return stale_jobs;
}

std::vector<nlohmann::json> InMemoryJobRepository::get_jobs_to_timeout(int timeout_minutes) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<nlohmann::json> result;
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
        const auto& job = it.value();
        std::string status = job.value("status", "");
        if (status == "assigned" || status == "processing") {
            int64_t updated_at = job.value("updated_at", 0LL);
            if ((now_ms - updated_at) / (1000 * 60) > timeout_minutes) {
                result.push_back(job);
            }
        }
    }
    return result;
}

bool InMemoryJobRepository::update_job(const std::string& job_id, const nlohmann::json& updates) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!jobs_.contains(job_id)) return false;
    
    for (auto it = updates.begin(); it != updates.end(); ++it) {
        jobs_[job_id][it.key()] = it.value();
    }
    jobs_[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return true;
}

std::vector<nlohmann::json> InMemoryJobRepository::get_jobs_by_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<nlohmann::json> result;
    for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
        if (it.value().value("assigned_engine", "") == engine_id) result.push_back(it.value());
    }
    return result;
}

bool InMemoryJobRepository::update_job_progress(const std::string& job_id, int progress, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!jobs_.contains(job_id)) return false;
    jobs_[job_id]["progress"] = progress;
    jobs_[job_id]["progress_message"] = message;
    jobs_[job_id]["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return true;
}

std::vector<nlohmann::json> InMemoryJobRepository::get_jobs_by_status(const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<nlohmann::json> result;
    for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
        if (it.value().value("status", "") == status) result.push_back(it.value());
    }
    return result;
}

std::vector<nlohmann::json> InMemoryJobRepository::get_timed_out_jobs(int64_t older_than_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<nlohmann::json> result;
    for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
        const auto& job = it.value();
        std::string status = job.value("status", "");
        if ((status == "assigned" || status == "processing") && job.value("updated_at", 0LL) < older_than_timestamp) {
            result.push_back(job);
        }
    }
    return result;
}

// InMemoryEngineRepository implementation
void InMemoryEngineRepository::save_engine(const std::string& engine_id, const nlohmann::json& engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    engines_[engine_id] = engine;
}

nlohmann::json InMemoryEngineRepository::get_engine(const std::string& engine_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return engines_.value(engine_id, nlohmann::json());
}

std::vector<nlohmann::json> InMemoryEngineRepository::get_all_engines() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<nlohmann::json> result;
    for (auto it = engines_.begin(); it != engines_.end(); ++it) {
        result.push_back(it.value());
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
    engines_ = nlohmann::json::object();
}

} // namespace DispatchServer
} // namespace distconv
