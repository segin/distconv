#include "sqlite_database.h"
#include <sqlite3.h>
#include <iostream>
#include <filesystem>

namespace distconv {
namespace TranscodingEngine {

class SqliteDatabase::Impl {
public:
    sqlite3* db_ = nullptr;
    std::string db_path_;
    
    ~Impl() {
        if (db_) {
            sqlite3_close(db_);
        }
    }
    
    bool execute_query(const std::string& query, std::vector<std::string>* results = nullptr) {
        char* error_message = nullptr;
        
        if (results) {
            // Query with results
            auto callback = [](void* data, int argc, char** argv, char** azColName) -> int {
                auto* result_vector = static_cast<std::vector<std::string>*>(data);
                for (int i = 0; i < argc; i++) {
                    if (std::string(azColName[i]) == "job_id" && argv[i]) {
                        result_vector->push_back(argv[i]);
                    }
                }
                return 0;
            };
            
            int rc = sqlite3_exec(db_, query.c_str(), callback, results, &error_message);
            
            if (rc != SQLITE_OK) {
                std::cerr << "SQL error: " << error_message << std::endl;
                sqlite3_free(error_message);
                return false;
            }
        } else {
            // Query without results
            int rc = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &error_message);
            
            if (rc != SQLITE_OK) {
                std::cerr << "SQL error: " << error_message << std::endl;
                sqlite3_free(error_message);
                return false;
            }
        }
        
        return true;
    }
    
    bool execute_prepared_statement(const std::string& query, const std::vector<std::string>& params) {
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr);
        
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }
        
        // Bind parameters
        for (size_t i = 0; i < params.size(); ++i) {
            rc = sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_STATIC);
            if (rc != SQLITE_OK) {
                std::cerr << "Failed to bind parameter " << i << ": " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_finalize(stmt);
                return false;
            }
        }
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }
        
        return true;
    }
};

SqliteDatabase::SqliteDatabase() : pimpl_(std::make_unique<Impl>()) {}

SqliteDatabase::~SqliteDatabase() {
    close();
}

bool SqliteDatabase::initialize(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    pimpl_->db_path_ = db_path;
    
    int rc = sqlite3_open(db_path.c_str(), &pimpl_->db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(pimpl_->db_) << std::endl;
        return false;
    }
    
    // Create jobs table with proper schema
    const std::string create_table_query = 
        "CREATE TABLE IF NOT EXISTS jobs("
        "job_id TEXT PRIMARY KEY NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";
    
    if (!pimpl_->execute_query(create_table_query)) {
        return false;
    }
    
    std::cout << "SQLite database initialized successfully: " << db_path << std::endl;
    return true;
}

bool SqliteDatabase::add_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!pimpl_->db_) {
        return false;
    }
    
    const std::string query = "INSERT OR IGNORE INTO jobs (job_id) VALUES (?);";
    return pimpl_->execute_prepared_statement(query, {job_id});
}

bool SqliteDatabase::remove_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!pimpl_->db_) {
        return false;
    }
    
    const std::string query = "DELETE FROM jobs WHERE job_id = ?;";
    return pimpl_->execute_prepared_statement(query, {job_id});
}

std::vector<std::string> SqliteDatabase::get_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> jobs;
    
    if (!pimpl_->db_) {
        return jobs;
    }
    
    const std::string query = "SELECT job_id FROM jobs ORDER BY created_at;";
    pimpl_->execute_query(query, &jobs);
    
    return jobs;
}

bool SqliteDatabase::job_exists(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!pimpl_->db_) {
        return false;
    }
    
    sqlite3_stmt* stmt;
    const std::string query = "SELECT COUNT(*) FROM jobs WHERE job_id = ?;";
    
    int rc = sqlite3_prepare_v2(pimpl_->db_, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    bool exists = false;
    
    if (rc == SQLITE_ROW) {
        int count = sqlite3_column_int(stmt, 0);
        exists = (count > 0);
    }
    
    sqlite3_finalize(stmt);
    return exists;
}

size_t SqliteDatabase::get_job_count() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!pimpl_->db_) {
        return 0;
    }
    
    sqlite3_stmt* stmt;
    const std::string query = "SELECT COUNT(*) FROM jobs;";
    
    int rc = sqlite3_prepare_v2(pimpl_->db_, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }
    
    rc = sqlite3_step(stmt);
    size_t count = 0;
    
    if (rc == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int(stmt, 0));
    }
    
    sqlite3_finalize(stmt);
    return count;
}

bool SqliteDatabase::clear_all_jobs() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!pimpl_->db_) {
        return false;
    }
    
    const std::string query = "DELETE FROM jobs;";
    return pimpl_->execute_query(query);
}

void SqliteDatabase::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pimpl_->db_) {
        sqlite3_close(pimpl_->db_);
        pimpl_->db_ = nullptr;
    }
}

bool SqliteDatabase::is_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pimpl_->db_ != nullptr;
}

} // namespace TranscodingEngine
} // namespace distconv
