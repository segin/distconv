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

// Helper function to clear the database before each test
inline void clear_db() {
    std::lock_guard<std::mutex> lock(state_mutex);
    jobs_db = nlohmann::json::object();
    engines_db = nlohmann::json::object();
}

// Test fixture for API tests
class ApiTest : public ::testing::Test {
protected:
    DispatchServer *server;
    httplib::Client *client;
    int port = 8081; // Use a different port to avoid conflicts
    std::string api_key = "test_api_key";
    std::string persistent_storage_file;

    void SetUp() override {
        // Generate a unique filename for the persistent storage file for each test
        persistent_storage_file = "dispatch_server_state_" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) + ".json";
        PERSISTENT_STORAGE_FILE = persistent_storage_file;

        clear_db(); 
        
        server = new DispatchServer(api_key);
        server->start(port, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give the server a moment to start
        client = new httplib::Client("localhost", port);
        client->set_connection_timeout(10); // Increased timeout
        wait_for_server_ready();
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