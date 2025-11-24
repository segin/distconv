#include "test_common.h"
#include <fstream>
#include <sqlite3.h>
#include <random>
#include "../transcoding_engine_core.h"

namespace distconv {
namespace TranscodingEngine {

bool is_valid_json(const std::string& json_str) {
    if (json_str.empty()) {
        return false;
    }
    
    // Basic JSON validation - starts and ends with proper brackets
    std::string trimmed = json_str;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
    
    if (trimmed.empty()) {
        return false;
    }
    
    // Should start with { or [
    if (trimmed[0] != '{' && trimmed[0] != '[') {
        return false;
    }
    
    // Should end with } or ]
    char last_char = trimmed[trimmed.length() - 1];
    if (last_char != '}' && last_char != ']') {
        return false;
    }
    
    // Basic bracket matching
    int brace_count = 0;
    int bracket_count = 0;
    bool in_string = false;
    bool escaped = false;
    
    for (char c : trimmed) {
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (c == '\\') {
            escaped = true;
            continue;
        }
        
        if (c == '"') {
            in_string = !in_string;
            continue;
        }
        
        if (in_string) {
            continue;
        }
        
        if (c == '{') {
            brace_count++;
        } else if (c == '}') {
            brace_count--;
        } else if (c == '[') {
            bracket_count++;
        } else if (c == ']') {
            bracket_count--;
        }
        
        if (brace_count < 0 || bracket_count < 0) {
            return false;
        }
    }
    
    return brace_count == 0 && bracket_count == 0;
}

std::string create_temp_file(const std::string& content, const std::string& suffix) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::string filename = "test_temp_" + std::to_string(dis(gen)) + suffix;
    
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
    }
    
    return filename;
}

bool verify_sqlite_schema(const std::string& db_path) {
    sqlite3* db;
    int rc = sqlite3_open(db_path.c_str(), &db);
    
    if (rc != SQLITE_OK) {
        return false;
    }
    
    // Check if jobs table exists
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='jobs';";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }
    
    rc = sqlite3_step(stmt);
    bool table_exists = (rc == SQLITE_ROW);
    
    sqlite3_finalize(stmt);
    
    // Check the schema of the jobs table
    if (table_exists) {
        const char* schema_sql = "PRAGMA table_info(jobs);";
        rc = sqlite3_prepare_v2(db, schema_sql, -1, &stmt, nullptr);
        
        if (rc == SQLITE_OK) {
            bool has_job_id_column = false;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* column_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if (column_name && std::string(column_name) == "job_id") {
                    has_job_id_column = true;
                    break;
                }
            }
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return has_job_id_column;
        }
    }
    
    sqlite3_close(db);
    return false;
}

} // namespace TranscodingEngine
} // namespace distconv
