#ifndef DISPATCH_SERVER_CORE_H
#define DISPATCH_SERVER_CORE_H

#include <string>
#include "nlohmann/json.hpp"

// Global API Key
extern std::string API_KEY;

// In-memory storage for jobs and engines
extern nlohmann::json jobs_db;
extern nlohmann::json engines_db;

// Persistent storage file
extern const std::string PERSISTENT_STORAGE_FILE;

// Function declarations
void load_state();
void save_state();
int run_dispatch_server(int argc, char* argv[]);

#endif // DISPATCH_SERVER_CORE_H
