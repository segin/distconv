#ifndef MOCK_DATABASE_H
#define MOCK_DATABASE_H

#include "../interfaces/database_interface.h"
#include <set>

namespace transcoding_engine {

class MockDatabase : public IDatabase {
public:
    MockDatabase() = default;
    ~MockDatabase() override = default;
    
    // IDatabase interface
    bool initialize(const std::string& db_path) override;
    bool add_job(const std::string& job_id) override;
    bool remove_job(const std::string& job_id) override;
    std::vector<std::string> get_all_jobs() override;
    bool job_exists(const std::string& job_id) override;
    size_t get_job_count() override;
    bool clear_all_jobs() override;
    void close() override;
    bool is_connected() const override;
    
    // Mock-specific methods
    void set_initialize_result(bool result) { initialize_result_ = result; }
    void set_add_job_result(bool result) { add_job_result_ = result; }
    void set_remove_job_result(bool result) { remove_job_result_ = result; }
    void set_connected_state(bool connected) { is_connected_ = connected; }
    
    // State inspection
    const std::string& get_db_path() const { return db_path_; }
    const std::set<std::string>& get_jobs_set() const { return jobs_; }
    int get_initialize_call_count() const { return initialize_call_count_; }
    int get_add_job_call_count() const { return add_job_call_count_; }
    int get_remove_job_call_count() const { return remove_job_call_count_; }
    int get_clear_call_count() const { return clear_call_count_; }
    
    void reset_call_counts() {
        initialize_call_count_ = 0;
        add_job_call_count_ = 0;
        remove_job_call_count_ = 0;
        clear_call_count_ = 0;
    }

private:
    std::set<std::string> jobs_;
    std::string db_path_;
    bool is_connected_ = false;
    
    // Mock behavior controls
    bool initialize_result_ = true;
    bool add_job_result_ = true;
    bool remove_job_result_ = true;
    
    // Call tracking
    int initialize_call_count_ = 0;
    int add_job_call_count_ = 0;
    int remove_job_call_count_ = 0;
    int clear_call_count_ = 0;
};

} // namespace transcoding_engine

#endif // MOCK_DATABASE_H