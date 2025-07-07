#ifndef DISPATCH_SERVER_CORE_H
#define DISPATCH_SERVER_CORE_H

#include <string>
#include "nlohmann/json.hpp"
#include "httplib.h"
#include <mutex>

// In-memory storage for jobs and engines
extern nlohmann::json jobs_db;
extern nlohmann::json engines_db;
extern std::mutex state_mutex;

// Persistent storage file
extern std::string PERSISTENT_STORAGE_FILE;

// Global flag and counter for mocking save_state
extern bool mock_save_state_enabled;
extern int save_state_call_count;

// Global flag and counter for mocking save_state
extern bool mock_save_state_enabled;
extern int save_state_call_count;

class DispatchServer {
public:
    DispatchServer();
    void start(int port, bool block = true);
    void stop();
    httplib::Server* getServer();
    void set_api_key(const std::string& key);
private:
    httplib::Server svr;
    std::thread server_thread;
    std::string api_key_;
};

// Function declarations
void load_state();
void save_state();
void setup_endpoints(httplib::Server &svr, const std::string& api_key);
DispatchServer* run_dispatch_server(int argc, char* argv[], DispatchServer* server_instance);
DispatchServer* run_dispatch_server(DispatchServer* server_instance);

#endif // DISPATCH_SERVER_CORE_H
