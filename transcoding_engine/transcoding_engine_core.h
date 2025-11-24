#ifndef TRANSCODING_ENGINE_CORE_H
#define TRANSCODING_ENGINE_CORE_H

#include <string>
#include <vector>
#include "cjson/cJSON.h"
#include <sqlite3.h>

namespace distconv {
namespace TranscodingEngine {

// Global SQLite database handle
extern sqlite3 *db;

// Function declarations
std::string getFFmpegCapabilities(const std::string& capability_type);
std::string getFFmpegHWAccels();
double getCpuTemperature();
std::string getHostname();
void sendHeartbeat(const std::string& dispatchServerUrl, const std::string& engineId, double storageCapacityGb, bool streamingSupport, const std::string& encoders, const std::string& decoders, const std::string& hwaccels, double cpuTemperature, const std::string& localJobQueue, const std::string& caCertPath, const std::string& apiKey, const std::string& hostname);
double performBenchmark();
void sendBenchmarkResult(const std::string& dispatchServerUrl, const std::string& engineId, double benchmark_time, const std::string& caCertPath, const std::string& apiKey);
std::string getJob(const std::string& dispatchServerUrl, const std::string& engineId, const std::string& caCertPath, const std::string& apiKey);
bool downloadFile(const std::string& url, const std::string& output_path, const std::string& caCertPath, const std::string& apiKey);
bool uploadFile(const std::string& url, const std::string& file_path, const std::string& caCertPath, const std::string& apiKey);
void reportJobStatus(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& status, const std::string& output_url, const std::string& error_message, const std::string& caCertPath, const std::string& apiKey);
void performStreamingTranscoding(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& source_url, const std::string& target_codec, const std::string& caCertPath, const std::string& apiKey);
void performTranscoding(const std::string& dispatchServerUrl, const std::string& job_id, const std::string& source_url, const std::string& target_codec, const std::string& caCertPath, const std::string& apiKey);

// SQLite functions
void init_sqlite();
void add_job_to_db(const std::string& job_id);
void remove_job_from_db(const std::string& job_id);
std::vector<std::string> get_jobs_from_db();

// Main application function
int run_transcoding_engine(int argc, char* argv[]);

} // namespace TranscodingEngine
} // namespace distconv

#endif // TRANSCODING_ENGINE_CORE_H
