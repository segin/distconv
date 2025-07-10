#include "transcoding_engine.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>
#include <chrono>
#include <unistd.h>
#include <sstream>
#include <optional>

namespace transcoding_engine {

TranscodingEngine::TranscodingEngine(std::unique_ptr<IHttpClient> http_client,
                                   std::unique_ptr<IDatabase> database,
                                   std::unique_ptr<ISubprocessRunner> subprocess_runner)
    : http_client_(std::move(http_client))
    , database_(std::move(database))
    , subprocess_runner_(std::move(subprocess_runner)) {
}

TranscodingEngine::~TranscodingEngine() {
    stop();
}

bool TranscodingEngine::initialize(const EngineConfig& config) {
    config_ = config;
    
    // Generate unique engine ID if not provided
    if (config_.engine_id.empty()) {
        std::random_device rd;
        config_.engine_id = "engine-" + std::to_string(rd() % 10000);
    }
    
    // Get hostname if not provided
    if (config_.hostname.empty()) {
        config_.hostname = get_hostname();
    }
    
    // Initialize database
    if (!database_->initialize(config_.database_path)) {
        std::cerr << "Failed to initialize database" << std::endl;
        return false;
    }
    
    // Configure HTTP client
    http_client_->set_ssl_options(config_.ca_cert_path, !config_.ca_cert_path.empty());
    http_client_->set_timeout(config_.http_timeout_seconds);
    
    // Verify ffmpeg is available
    if (!subprocess_runner_->is_executable_available("ffmpeg")) {
        std::cerr << "FFmpeg not found - transcoding will not work" << std::endl;
        if (!config_.test_mode) {
            return false;
        }
    }
    
    std::cout << "Transcoding Engine initialized: " << config_.engine_id << std::endl;
    return true;
}

bool TranscodingEngine::start() {
    if (running_.load()) {
        return false; // Already running
    }
    
    running_.store(true);
    
    if (!config_.test_mode) {
        // Start background threads
        heartbeat_thread_ = std::thread(&TranscodingEngine::heartbeat_loop, this);
        benchmark_thread_ = std::thread(&TranscodingEngine::benchmark_loop, this);
        main_loop_thread_ = std::thread(&TranscodingEngine::main_job_loop, this);
    }
    
    std::cout << "Transcoding Engine started" << std::endl;
    return true;
}

void TranscodingEngine::stop() {
    if (!running_.load()) {
        return; // Already stopped
    }
    
    running_.store(false);
    
    // Wait for threads to finish
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    if (benchmark_thread_.joinable()) {
        benchmark_thread_.join();
    }
    if (main_loop_thread_.joinable()) {
        main_loop_thread_.join();
    }
    
    database_->close();
    std::cout << "Transcoding Engine stopped" << std::endl;
}

bool TranscodingEngine::is_running() const {
    return running_.load();
}

bool TranscodingEngine::register_with_dispatcher() {
    auto queued_jobs = get_queued_jobs();
    
    nlohmann::json heartbeat_data = {
        {"engine_id", config_.engine_id},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "h265", "vp8", "vp9"}},
        {"status", "idle"},
        {"storage_capacity_gb", config_.storage_capacity_gb},
        {"streaming_support", config_.streaming_support},
        {"hostname", config_.hostname},
        {"local_job_queue", queued_jobs}
    };
    
    auto headers = create_auth_headers();
    headers["Content-Type"] = "application/json";
    
    std::string url = config_.dispatch_server_url + "/engines/heartbeat";
    auto response = http_client_->post(url, heartbeat_data.dump(), headers);
    
    if (response.success) {
        std::cout << "Successfully registered with dispatcher" << std::endl;
        return true;
    } else {
        std::cerr << "Failed to register with dispatcher: " << response.error_message << std::endl;
        return false;
    }
}

std::optional<JobDetails> TranscodingEngine::get_job_from_dispatcher() {
    nlohmann::json request_data = {
        {"engine_id", config_.engine_id}
    };
    
    auto headers = create_auth_headers();
    headers["Content-Type"] = "application/json";
    
    std::string url = config_.dispatch_server_url + "/assign_job/";
    auto response = http_client_->post(url, request_data.dump(), headers);
    
    if (!response.success) {
        if (response.status_code != 204) { // 204 = No Content (no jobs available)
            std::cerr << "Failed to get job from dispatcher: " << response.error_message << std::endl;
        }
        return std::nullopt;
    }
    
    if (response.status_code == 204 || response.body.empty()) {
        return std::nullopt; // No jobs available
    }
    
    try {
        auto job_json = nlohmann::json::parse(response.body);
        
        JobDetails job;
        job.job_id = job_json.value("job_id", "");
        job.source_url = job_json.value("source_url", "");
        job.target_codec = job_json.value("target_codec", "");
        job.job_size = job_json.value("job_size", 0.0);
        
        // Validate required fields
        if (job.job_id.empty() || job.source_url.empty() || job.target_codec.empty()) {
            std::cerr << "Invalid job data received from dispatcher" << std::endl;
            return std::nullopt;
        }
        
        return job;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Failed to parse job JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool TranscodingEngine::process_job(const JobDetails& job) {
    std::cout << "Processing job: " << job.job_id << std::endl;
    
    // Add to local queue
    if (!add_job_to_queue(job.job_id)) {
        std::cerr << "Failed to add job to local queue" << std::endl;
        return false;
    }
    
    // Generate temporary file names
    std::string input_file = generate_unique_filename(job.job_id, ".input.mp4");
    std::string output_file = generate_unique_filename(job.job_id, ".output.mp4");
    
    std::vector<std::string> temp_files = {input_file, output_file};
    
    try {
        // Step 1: Download source file
        if (!download_source_file(job.source_url, input_file)) {
            report_job_failure(job.job_id, "Failed to download source video");
            cleanup_temp_files(temp_files);
            remove_job_from_queue(job.job_id);
            return false;
        }
        
        // Step 2: Transcode
        if (!transcode_file(input_file, output_file, job.target_codec)) {
            report_job_failure(job.job_id, "FFmpeg transcoding failed");
            cleanup_temp_files(temp_files);
            remove_job_from_queue(job.job_id);
            return false;
        }
        
        // Step 3: Upload result
        std::string upload_url = "http://example.com/transcoded/" + job.job_id + ".mp4";
        if (!upload_result_file(output_file, upload_url)) {
            report_job_failure(job.job_id, "Failed to upload transcoded video");
            cleanup_temp_files(temp_files);
            remove_job_from_queue(job.job_id);
            return false;
        }
        
        // Step 4: Report completion
        if (!report_job_completion(job.job_id, upload_url)) {
            std::cerr << "Failed to report job completion (job completed but not reported)" << std::endl;
        }
        
        // Cleanup
        cleanup_temp_files(temp_files);
        remove_job_from_queue(job.job_id);
        
        std::cout << "Successfully processed job: " << job.job_id << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        handle_error("process_job", e.what());
        report_job_failure(job.job_id, std::string("Exception during processing: ") + e.what());
        cleanup_temp_files(temp_files);
        remove_job_from_queue(job.job_id);
        return false;
    }
}

bool TranscodingEngine::report_job_completion(const std::string& job_id, const std::string& output_url) {
    nlohmann::json completion_data = {
        {"output_url", output_url}
    };
    
    auto headers = create_auth_headers();
    headers["Content-Type"] = "application/json";
    
    std::string url = config_.dispatch_server_url + "/jobs/" + job_id + "/complete";
    auto response = http_client_->post(url, completion_data.dump(), headers);
    
    if (response.success) {
        std::cout << "Reported job completion: " << job_id << std::endl;
        return true;
    } else {
        std::cerr << "Failed to report job completion: " << response.error_message << std::endl;
        return false;
    }
}

bool TranscodingEngine::report_job_failure(const std::string& job_id, const std::string& error_message) {
    nlohmann::json failure_data = {
        {"error_message", error_message}
    };
    
    auto headers = create_auth_headers();
    headers["Content-Type"] = "application/json";
    
    std::string url = config_.dispatch_server_url + "/jobs/" + job_id + "/fail";
    auto response = http_client_->post(url, failure_data.dump(), headers);
    
    if (response.success) {
        std::cout << "Reported job failure: " << job_id << " - " << error_message << std::endl;
        return true;
    } else {
        std::cerr << "Failed to report job failure: " << response.error_message << std::endl;
        return false;
    }
}

std::string TranscodingEngine::get_ffmpeg_capabilities(const std::string& capability_type) {
    std::vector<std::string> command = {"ffmpeg", "-hide_banner", "-" + capability_type};
    auto result = subprocess_runner_->run(command);
    
    if (result.success) {
        // Parse output to extract codec names
        std::istringstream stream(result.stdout_output);
        std::string line;
        std::vector<std::string> codecs;
        
        while (std::getline(stream, line)) {
            if (line.find("DEV") != std::string::npos || line.find("D.V") != std::string::npos) {
                // Extract codec name (second column)
                std::istringstream line_stream(line);
                std::string flags, codec_name;
                if (line_stream >> flags >> codec_name) {
                    codecs.push_back(codec_name);
                }
            }
        }
        
        // Join codec names with commas
        std::ostringstream result_stream;
        for (size_t i = 0; i < codecs.size(); ++i) {
            if (i > 0) result_stream << ",";
            result_stream << codecs[i];
        }
        return result_stream.str();
    }
    
    return "";
}

std::string TranscodingEngine::get_ffmpeg_hw_accels() {
    std::vector<std::string> command = {"ffmpeg", "-hide_banner", "-hwaccels"};
    auto result = subprocess_runner_->run(command);
    
    if (result.success) {
        // Parse output to extract hardware acceleration methods
        std::istringstream stream(result.stdout_output);
        std::string line;
        std::vector<std::string> hwaccels;
        
        while (std::getline(stream, line)) {
            // Skip header lines
            if (line.find("Hardware acceleration methods:") == std::string::npos && 
                !line.empty() && line != "Hardware acceleration methods:") {
                hwaccels.push_back(line);
            }
        }
        
        // Join with commas
        std::ostringstream result_stream;
        for (size_t i = 0; i < hwaccels.size(); ++i) {
            if (i > 0) result_stream << ",";
            result_stream << hwaccels[i];
        }
        return result_stream.str();
    }
    
    return "";
}

double TranscodingEngine::get_cpu_temperature() {
#ifdef __linux__
    std::ifstream thermal_file("/sys/class/thermal/thermal_zone0/temp");
    if (thermal_file.is_open()) {
        std::string line;
        if (std::getline(thermal_file, line)) {
            try {
                return std::stod(line) / 1000.0; // Convert millidegrees to Celsius
            } catch (const std::exception&) {
                return -1.0;
            }
        }
    }
#endif
    return -1.0; // Not supported or error
}

double TranscodingEngine::run_benchmark() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Simple benchmark: run ffmpeg version command
    std::vector<std::string> command = {"ffmpeg", "-version"};
    subprocess_runner_->run(command);
    
    // Simulate some processing time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return duration.count() / 1000.0; // Return seconds
}

bool TranscodingEngine::send_benchmark_result(double benchmark_time) {
    nlohmann::json benchmark_data = {
        {"engine_id", config_.engine_id},
        {"benchmark_time", benchmark_time}
    };
    
    auto headers = create_auth_headers();
    headers["Content-Type"] = "application/json";
    
    std::string url = config_.dispatch_server_url + "/engines/benchmark_result";
    auto response = http_client_->post(url, benchmark_data.dump(), headers);
    
    return response.success;
}

bool TranscodingEngine::add_job_to_queue(const std::string& job_id) {
    return database_->add_job(job_id);
}

bool TranscodingEngine::remove_job_from_queue(const std::string& job_id) {
    return database_->remove_job(job_id);
}

std::vector<std::string> TranscodingEngine::get_queued_jobs() {
    return database_->get_all_jobs();
}

const EngineConfig& TranscodingEngine::get_config() const {
    return config_;
}

nlohmann::json TranscodingEngine::get_status() const {
    auto queued_jobs = database_->get_all_jobs();
    
    return {
        {"engine_id", config_.engine_id},
        {"hostname", config_.hostname},
        {"running", running_.load()},
        {"queued_jobs", queued_jobs},
        {"job_count", queued_jobs.size()},
        {"database_connected", database_->is_connected()}
    };
}

std::string TranscodingEngine::get_hostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "unknown";
}

// Private methods

void TranscodingEngine::heartbeat_loop() {
    while (running_.load()) {
        send_heartbeat();
        
        for (int i = 0; i < config_.heartbeat_interval_seconds && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void TranscodingEngine::benchmark_loop() {
    while (running_.load()) {
        double benchmark_time = run_benchmark();
        send_benchmark_result(benchmark_time);
        
        for (int i = 0; i < config_.benchmark_interval_minutes * 60 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void TranscodingEngine::main_job_loop() {
    while (running_.load()) {
        auto job = get_job_from_dispatcher();
        if (job.has_value()) {
            process_job(job.value());
        }
        
        for (int i = 0; i < config_.job_poll_interval_seconds && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

bool TranscodingEngine::send_heartbeat() {
    auto queued_jobs = get_queued_jobs();
    
    nlohmann::json heartbeat_data = {
        {"engine_id", config_.engine_id},
        {"engine_type", "transcoder"},
        {"status", "idle"},
        {"storage_capacity_gb", config_.storage_capacity_gb},
        {"streaming_support", config_.streaming_support},
        {"encoders", get_ffmpeg_capabilities("encoders")},
        {"decoders", get_ffmpeg_capabilities("decoders")},
        {"hwaccels", get_ffmpeg_hw_accels()},
        {"cpu_temperature", get_cpu_temperature()},
        {"local_job_queue", queued_jobs},
        {"hostname", config_.hostname}
    };
    
    auto headers = create_auth_headers();
    headers["Content-Type"] = "application/json";
    
    std::string url = config_.dispatch_server_url + "/engines/heartbeat";
    auto response = http_client_->post(url, heartbeat_data.dump(), headers);
    
    return response.success;
}

bool TranscodingEngine::download_source_file(const std::string& source_url, const std::string& output_path) {
    auto headers = create_auth_headers();
    auto response = http_client_->download_file(source_url, output_path, headers);
    
    if (response.success && std::filesystem::exists(output_path)) {
        std::cout << "Downloaded source file: " << output_path << std::endl;
        return true;
    } else {
        std::cerr << "Failed to download source file: " << response.error_message << std::endl;
        return false;
    }
}

bool TranscodingEngine::transcode_file(const std::string& input_path, const std::string& output_path, 
                                      const std::string& target_codec) {
    std::vector<std::string> command = {
        "ffmpeg", "-y", // Overwrite output files
        "-i", input_path,
        "-c:v", target_codec,
        output_path
    };
    
    auto result = subprocess_runner_->run(command);
    
    if (result.success && std::filesystem::exists(output_path)) {
        std::cout << "Transcoded file successfully: " << output_path << std::endl;
        return true;
    } else {
        std::cerr << "FFmpeg transcoding failed: " << result.stderr_output << std::endl;
        return false;
    }
}

bool TranscodingEngine::upload_result_file(const std::string& file_path, const std::string& upload_url) {
    auto headers = create_auth_headers();
    auto response = http_client_->upload_file(upload_url, file_path, headers);
    
    if (response.success) {
        std::cout << "Uploaded result file to: " << upload_url << std::endl;
        return true;
    } else {
        std::cerr << "Failed to upload result file: " << response.error_message << std::endl;
        return false;
    }
}

std::map<std::string, std::string> TranscodingEngine::create_auth_headers() const {
    std::map<std::string, std::string> headers;
    if (!config_.api_key.empty()) {
        headers["X-API-Key"] = config_.api_key;
    }
    return headers;
}

std::string TranscodingEngine::generate_unique_filename(const std::string& job_id, const std::string& extension) {
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return job_id + "_" + std::to_string(timestamp) + extension;
}

bool TranscodingEngine::cleanup_temp_files(const std::vector<std::string>& file_paths) {
    bool all_cleaned = true;
    for (const auto& file_path : file_paths) {
        if (std::filesystem::exists(file_path)) {
            try {
                std::filesystem::remove(file_path);
                std::cout << "Cleaned up temp file: " << file_path << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to cleanup temp file " << file_path << ": " << e.what() << std::endl;
                all_cleaned = false;
            }
        }
    }
    return all_cleaned;
}

void TranscodingEngine::handle_error(const std::string& context, const std::string& error_message) {
    std::cerr << "Error in " << context << ": " << error_message << std::endl;
}

bool TranscodingEngine::is_recoverable_error(const std::string& error_message) {
    // Define recoverable error patterns
    std::vector<std::string> recoverable_patterns = {
        "network timeout",
        "connection refused",
        "temporary failure"
    };
    
    for (const auto& pattern : recoverable_patterns) {
        if (error_message.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

} // namespace transcoding_engine