#include "job_scheduler.h"
#include <algorithm>
#include <cmath>

extern nlohmann::json jobs_db;
extern nlohmann::json engines_db;
extern std::mutex state_mutex;

JobScheduler::JobScheduler() {
    engine_cache_updated_ = std::chrono::system_clock::time_point::min();
}

std::string JobScheduler::get_next_pending_job() {
    std::lock_guard<std::mutex> scheduler_lock(scheduler_mutex_);
    
    // Check retry jobs first
    auto now = std::chrono::system_clock::now();
    for (auto it = retry_jobs_.begin(); it != retry_jobs_.end(); ++it) {
        if (now >= it->retry_after) {
            std::string job_id = it->job_id;
            retry_jobs_.erase(it);
            
            // Add back to pending queue with current time
            std::lock_guard<std::mutex> state_lock(state_mutex);
            if (jobs_db.contains(job_id)) {
                int priority = jobs_db[job_id].value("priority", 0);
                pending_jobs_.push({job_id, priority, now});
            }
            break;
        }
    }
    
    // Get next pending job by priority
    while (!pending_jobs_.empty()) {
        PendingJob next_job = pending_jobs_.top();
        pending_jobs_.pop();
        
        // Verify job still exists and is pending
        std::lock_guard<std::mutex> state_lock(state_mutex);
        if (jobs_db.contains(next_job.job_id) && 
            jobs_db[next_job.job_id]["status"] == "pending") {
            return next_job.job_id;
        }
    }
    
    return "";
}

void JobScheduler::add_job_to_queue(const std::string& job_id, int priority) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    pending_jobs_.push({job_id, priority, std::chrono::system_clock::now()});
}

void JobScheduler::remove_job_from_queue(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    
    // Remove from retry jobs
    retry_jobs_.erase(
        std::remove_if(retry_jobs_.begin(), retry_jobs_.end(),
                      [&job_id](const RetryJob& rj) { return rj.job_id == job_id; }),
        retry_jobs_.end());
    
    // Note: Can't efficiently remove from priority_queue, but get_next_pending_job() 
    // will filter out invalid jobs
}

std::string JobScheduler::select_best_engine_for_job(const std::string& job_id) {
    update_engine_cache();
    
    std::lock_guard<std::mutex> state_lock(state_mutex);
    if (!jobs_db.contains(job_id)) return "";
    
    std::string best_engine;
    double best_score = -1.0;
    
    for (const auto& engine_id : sorted_engines_) {
        if (engines_db.contains(engine_id) && 
            engines_db[engine_id]["status"] == "idle") {
            
            double score = calculate_engine_score(engine_id, job_id);
            if (score > best_score) {
                best_score = score;
                best_engine = engine_id;
            }
        }
    }
    
    return best_engine;
}

void JobScheduler::schedule_job_retry(const std::string& job_id, int retry_count) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    
    auto retry_delay = calculate_retry_delay(retry_count);
    auto retry_after = std::chrono::system_clock::now() + retry_delay;
    
    retry_jobs_.push_back({job_id, retry_after, retry_count});
}

bool JobScheduler::is_job_ready_for_retry(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    auto now = std::chrono::system_clock::now();
    
    for (const auto& retry_job : retry_jobs_) {
        if (retry_job.job_id == job_id) {
            return now >= retry_job.retry_after;
        }
    }
    return false;
}

void JobScheduler::expire_old_pending_jobs(std::chrono::minutes max_age) {
    std::lock_guard<std::mutex> state_lock(state_mutex);
    auto now = std::chrono::system_clock::now();
    
    for (auto& [job_id, job_data] : jobs_db.items()) {
        if (job_data["status"] == "pending" && job_data.contains("created_at")) {
            auto created_at = std::chrono::system_clock::time_point{
                std::chrono::milliseconds{job_data["created_at"]}
            };
            
            if (now - created_at > max_age) {
                job_data["status"] = "expired";
                job_data["updated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
                remove_job_from_queue(job_id);
            }
        }
    }
}

void JobScheduler::update_engine_cache() {
    auto now = std::chrono::system_clock::now();
    if (now - engine_cache_updated_ < std::chrono::minutes(1)) {
        return; // Cache is still fresh
    }
    
    std::lock_guard<std::mutex> state_lock(state_mutex);
    sorted_engines_.clear();
    
    // Collect and sort engines by last heartbeat (most recent first)
    std::vector<std::pair<std::string, int64_t>> engine_heartbeats;
    for (const auto& [engine_id, engine_data] : engines_db.items()) {
        int64_t last_heartbeat = engine_data.value("last_heartbeat", 0);
        engine_heartbeats.emplace_back(engine_id, last_heartbeat);
    }
    
    std::sort(engine_heartbeats.begin(), engine_heartbeats.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second; // Most recent first
              });
    
    for (const auto& [engine_id, _] : engine_heartbeats) {
        sorted_engines_.push_back(engine_id);
    }
    
    engine_cache_updated_ = now;
}

std::vector<std::string> JobScheduler::get_sorted_engines() {
    update_engine_cache();
    return sorted_engines_;
}

double JobScheduler::calculate_engine_score(const std::string& engine_id, const std::string& job_id) {
    if (!engines_db.contains(engine_id) || !jobs_db.contains(job_id)) {
        return 0.0;
    }
    
    const auto& engine_data = engines_db[engine_id];
    const auto& job_data = jobs_db[job_id];
    
    double score = 100.0; // Base score
    
    // Factor in benchmark time (lower is better)
    double benchmark_time = engine_data.value("benchmark_time", 100.0);
    if (benchmark_time > 0) {
        score += (100.0 / benchmark_time); // Faster engines get higher score
    }
    
    // Factor in job size and engine capabilities
    double job_size = job_data.value("job_size", 0.0);
    bool can_stream = engine_data.value("can_stream", false);
    
    // Large jobs benefit from streaming capability
    if (job_size > 100.0 && can_stream) {
        score += 20.0;
    }
    
    // Factor in storage capacity
    int storage_capacity = engine_data.value("storage_capacity_gb", 0);
    if (storage_capacity > job_size * 2) { // Has enough space
        score += 10.0;
    }
    
    // Recent heartbeat bonus
    auto now = std::chrono::system_clock::now();
    int64_t last_heartbeat = engine_data.value("last_heartbeat", 0);
    auto heartbeat_time = std::chrono::system_clock::time_point{
        std::chrono::milliseconds{last_heartbeat}
    };
    auto heartbeat_age = std::chrono::duration_cast<std::chrono::minutes>(now - heartbeat_time);
    
    if (heartbeat_age.count() < 1) {
        score += 15.0; // Very recent heartbeat
    } else if (heartbeat_age.count() < 5) {
        score += 5.0; // Recent heartbeat
    }
    
    return score;
}

std::chrono::minutes JobScheduler::calculate_retry_delay(int retry_count) {
    // Exponential backoff: 1, 2, 4, 8, 16 minutes, max 30 minutes
    int delay_minutes = std::min(static_cast<int>(std::pow(2, retry_count)), 30);
    return std::chrono::minutes(delay_minutes);
}