#ifndef SQLITE_DATABASE_H
#define SQLITE_DATABASE_H

#include "../interfaces/database_interface.h"
#include <memory>
#include <mutex>

namespace distconv {
namespace TranscodingEngine {

class SqliteDatabase : public IDatabase {
public:
    SqliteDatabase();
    ~SqliteDatabase() override;
    
    bool initialize(const std::string& db_path) override;
    bool add_job(const std::string& job_id) override;
    bool remove_job(const std::string& job_id) override;
    std::vector<std::string> get_all_jobs() override;
    bool job_exists(const std::string& job_id) override;
    size_t get_job_count() override;
    bool clear_all_jobs() override;
    void close() override;
    bool is_connected() const override;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    mutable std::mutex mutex_;
};

} // namespace TranscodingEngine
} // namespace distconv

#endif // SQLITE_DATABASE_H
