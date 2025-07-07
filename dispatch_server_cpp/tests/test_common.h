#ifndef DISPATCH_SERVER_TEST_COMMON_H
#define DISPATCH_SERVER_TEST_COMMON_H

#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream> // Required for std::ofstream
#include <cstdio> // Required for std::remove
#include <thread>
#include <chrono>
#include <random>

// Helper function to clear the database before each test
inline void clear_db() {
    std::lock_guard<std::mutex> lock(state_mutex);
    jobs_db = nlohmann::json::object();
    engines_db = nlohmann::json::object();
}

// Helper function to find an available port
inline int find_available_port() {
    // Use a socket to find an available port
    // This is a common trick to get a port that's likely free
    httplib::Server temp_svr;
    if (temp_svr.listen("127.0.0.1", 0)) { // Port 0 means bind to any available port
        int available_port = temp_svr.bind_to_any_port("127.0.0.1");
        temp_svr.stop(); // Stop the temporary server immediately
        if (available_port > 0) {
            return available_port;
        }
    }
    // Fallback to a default or throw an error if no port is found
    return 8081; // Fallback to default if dynamic port allocation fails
}

// Test fixture for API tests
class ApiTest : public ::testing::Test {
protected:
    DispatchServer *server;
    httplib::Client *client;
    int port;
    std::string api_key = "test_api_key";
    httplib::Headers admin_headers;
    std::string persistent_storage_file;

    void SetUp() override {
        // Generate a unique filename for the persistent storage file for each test
        persistent_storage_file = "dispatch_server_state_" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) + ".json";
        PERSISTENT_STORAGE_FILE = persistent_storage_file;

        clear_db(); 
        
        port = find_available_port(); // Dynamically find an available port

        server = new DispatchServer();
        server->set_api_key(api_key);
        server->start(port, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give the server a moment to start
        client = new httplib::Client("localhost", port);
        client->set_connection_timeout(30); // Increased timeout for stability
        wait_for_server_ready();

        // Initialize admin_headers here
        admin_headers = {
            {"Authorization", "some_token"},
            {"X-API-Key", api_key}
        };
    }

    void TearDown() override {
        server->stop();
        delete server;
        delete client;
        // Clean up the unique persistent storage file
        std::remove(persistent_storage_file.c_str());
        PERSISTENT_STORAGE_FILE = "dispatch_server_state.json"; // Reset to default
    }

    void wait_for_server_ready() {
        int retries = 10;
        while (retries-- > 0) {
            auto res = client->Get("/");
            if (res && res->status == 200) {
                return; // Server is ready
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        FAIL() << "Server did not become ready in time.";
    }
};

#endif // DISPATCH_SERVER_TEST_COMMON_H
