#ifndef JOB_SCHEDULER_H
#define JOB_SCHEDULER_H

#include "nlohmann/json.hpp"
#include <queue>
#include <vector>
#include <mutex>
#include <chrono>

class JobScheduler {
public:
    JobScheduler();
    
    // Priority-based job scheduling
    std::string get_next_pending_job();
    void add_job_to_queue(const std::string& job_id, int priority);
    void remove_job_from_queue(const std::string& job_id);
    
    // Engine selection
    std::string select_best_engine_for_job(const std::string& job_id);
    
    // Job retry logic with backoff
    void schedule_job_retry(const std::string& job_id, int retry_count);
    bool is_job_ready_for_retry(const std::string& job_id);
    
    // Job expiration
    void expire_old_pending_jobs(std::chrono::minutes max_age = std::chrono::minutes(60));
    
    // Engine sorting and caching
    void update_engine_cache();
    std::vector<std::string> get_sorted_engines();

private:
    struct PendingJob {
        std::string job_id;
        int priority;
        std::chrono::system_clock::time_point queued_at;
        
        // Higher priority jobs come first, then by submission time
        bool operator<(const PendingJob& other) const {
            if (priority != other.priority) {
                return priority < other.priority; // Higher priority = higher number
            }
            return queued_at > other.queued_at; // Earlier jobs first
        }
    };
    
    struct RetryJob {
        std::string job_id;
        std::chrono::system_clock::time_point retry_after;
        int retry_count;
    };
    
    std::priority_queue<PendingJob> pending_jobs_;
    std::vector<RetryJob> retry_jobs_;
    std::vector<std::string> sorted_engines_;
    std::chrono::system_clock::time_point engine_cache_updated_;
    std::mutex scheduler_mutex_;
    
    // Helper functions
    double calculate_engine_score(const std::string& engine_id, const std::string& job_id);
    std::chrono::minutes calculate_retry_delay(int retry_count);
};

#endif // JOB_SCHEDULER_H