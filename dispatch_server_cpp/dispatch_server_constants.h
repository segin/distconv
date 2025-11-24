#ifndef DISPATCH_SERVER_CONSTANTS_H
#define DISPATCH_SERVER_CONSTANTS_H

#include <chrono>
#include <string>

namespace Constants {

// Background worker intervals
constexpr std::chrono::seconds BACKGROUND_WORKER_INTERVAL{30};

// Engine timeout and cleanup
constexpr std::chrono::minutes ENGINE_HEARTBEAT_TIMEOUT{5};

// Job timeout
constexpr std::chrono::minutes JOB_TIMEOUT{30};

// Default retry limits
// Default retry limits
constexpr int DEFAULT_MAX_RETRIES = 3;
constexpr int MAX_RETRIES = 5; // Hard limit or default if not specified
constexpr int RETRY_DELAY_BASE_SECONDS = 30;

// Default persistent storage file
inline const std::string DEFAULT_STATE_FILE = "dispatch_server_state.json";

// Job priority levels
constexpr int PRIORITY_NORMAL = 0;
constexpr int PRIORITY_HIGH = 1;
constexpr int PRIORITY_URGENT = 2;

// Job size thresholds (in MB) for scheduling
constexpr double JOB_SIZE_SMALL_THRESHOLD = 50.0;
constexpr double JOB_SIZE_MEDIUM_THRESHOLD = 100.0;

}  // namespace Constants

#endif  // DISPATCH_SERVER_CONSTANTS_H
