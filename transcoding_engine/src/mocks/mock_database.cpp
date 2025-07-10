#include "mock_database.h"
#include <algorithm>

namespace transcoding_engine {

bool MockDatabase::initialize(const std::string& db_path) {
    initialize_call_count_++;
    db_path_ = db_path;
    is_connected_ = initialize_result_;
    return initialize_result_;
}

bool MockDatabase::add_job(const std::string& job_id) {
    add_job_call_count_++;
    if (add_job_result_) {
        jobs_.insert(job_id);
    }
    return add_job_result_;
}

bool MockDatabase::remove_job(const std::string& job_id) {
    remove_job_call_count_++;
    if (remove_job_result_) {
        jobs_.erase(job_id);
    }
    return remove_job_result_;
}

std::vector<std::string> MockDatabase::get_all_jobs() {
    return std::vector<std::string>(jobs_.begin(), jobs_.end());
}

bool MockDatabase::job_exists(const std::string& job_id) {
    return jobs_.find(job_id) != jobs_.end();
}

size_t MockDatabase::get_job_count() {
    return jobs_.size();
}

bool MockDatabase::clear_all_jobs() {
    clear_call_count_++;
    jobs_.clear();
    return true;
}

void MockDatabase::close() {
    is_connected_ = false;
}

bool MockDatabase::is_connected() const {
    return is_connected_;
}

} // namespace transcoding_engine