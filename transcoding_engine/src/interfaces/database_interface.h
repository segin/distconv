#ifndef DATABASE_INTERFACE_H
#define DATABASE_INTERFACE_H

#include <string>
#include <vector>
#include <memory>

namespace distconv {
namespace TranscodingEngine {

class IDatabase {
public:
    virtual ~IDatabase() = default;
    
    virtual bool initialize(const std::string& db_path) = 0;
    virtual bool add_job(const std::string& job_id) = 0;
    virtual bool remove_job(const std::string& job_id) = 0;
    virtual std::vector<std::string> get_all_jobs() = 0;
    virtual bool job_exists(const std::string& job_id) = 0;
    virtual size_t get_job_count() = 0;
    virtual bool clear_all_jobs() = 0;
    virtual void close() = 0;
    virtual bool is_connected() const = 0;
};

} // namespace TranscodingEngine
} // namespace distconv

#endif // DATABASE_INTERFACE_H
