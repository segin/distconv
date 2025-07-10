#ifndef TRANSCODING_ENGINE_H
#define TRANSCODING_ENGINE_H

#include "../interfaces/http_client_interface.h"
#include "../interfaces/database_interface.h"
#include "../interfaces/subprocess_interface.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <optional>

namespace transcoding_engine {

struct EngineConfig {
    std::string dispatch_server_url = "http://localhost:8080";
    std::string engine_id;
    std::string api_key;
    std::string ca_cert_path;
    std::string hostname;
    std::string database_path = "transcoding_jobs.db";
    double storage_capacity_gb = 500.0;
    bool streaming_support = true;
    int heartbeat_interval_seconds = 5;
    int benchmark_interval_minutes = 5;
    int job_poll_interval_seconds = 1;
    int http_timeout_seconds = 30;
    bool test_mode = false;
};

struct JobDetails {
    std::string job_id;
    std::string source_url;
    std::string target_codec;
    double job_size = 0.0;
};

class TranscodingEngine {
public:
    TranscodingEngine(std::unique_ptr<IHttpClient> http_client,
                     std::unique_ptr<IDatabase> database,
                     std::unique_ptr<ISubprocessRunner> subprocess_runner);
    
    ~TranscodingEngine();
    
    bool initialize(const EngineConfig& config);
    bool start();
    void stop();
    bool is_running() const;
    
    // Core functionality
    bool register_with_dispatcher();
    std::optional<JobDetails> get_job_from_dispatcher();
    bool process_job(const JobDetails& job);
    bool report_job_completion(const std::string& job_id, const std::string& output_url);
    bool report_job_failure(const std::string& job_id, const std::string& error_message);
    
    // System monitoring
    std::string get_ffmpeg_capabilities(const std::string& capability_type);
    std::string get_ffmpeg_hw_accels();
    double get_cpu_temperature();
    double run_benchmark();
    bool send_benchmark_result(double benchmark_time);
    
    // Job queue management
    bool add_job_to_queue(const std::string& job_id);
    bool remove_job_from_queue(const std::string& job_id);
    std::vector<std::string> get_queued_jobs();
    
    // Configuration and status
    const EngineConfig& get_config() const;
    nlohmann::json get_status() const;
    std::string get_hostname();

private:
    // Core dependencies
    std::unique_ptr<IHttpClient> http_client_;
    std::unique_ptr<IDatabase> database_;
    std::unique_ptr<ISubprocessRunner> subprocess_runner_;
    
    // Configuration
    EngineConfig config_;
    
    // Threading
    std::atomic<bool> running_{false};
    std::thread heartbeat_thread_;
    std::thread benchmark_thread_;
    std::thread main_loop_thread_;
    
    // Internal methods
    void heartbeat_loop();
    void benchmark_loop();
    void main_job_loop();
    bool send_heartbeat();
    
    // Job processing
    bool download_source_file(const std::string& source_url, const std::string& output_path);
    bool transcode_file(const std::string& input_path, const std::string& output_path, 
                       const std::string& target_codec);
    bool upload_result_file(const std::string& file_path, const std::string& upload_url);
    
    // Utility methods
    std::map<std::string, std::string> create_auth_headers() const;
    std::string generate_unique_filename(const std::string& job_id, const std::string& extension);
    bool cleanup_temp_files(const std::vector<std::string>& file_paths);
    
    // Error handling
    void handle_error(const std::string& context, const std::string& error_message);
    bool is_recoverable_error(const std::string& error_message);
};

} // namespace transcoding_engine

#endif // TRANSCODING_ENGINE_H