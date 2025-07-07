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
    // Use a random port in the high range to avoid conflicts
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 65000);
    return dis(gen);
}

// Test fixture for API tests
class ApiTest : public ::testing::Test {
protected:
    static DispatchServer *server;
    static httplib::Client *client;
    static int port;
    static std::string api_key;
    static httplib::Headers admin_headers;
    static std::string persistent_storage_file;

    static void SetUpTestSuite() {
        persistent_storage_file = "dispatch_server_state_" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) + ".json";
        PERSISTENT_STORAGE_FILE = persistent_storage_file;

        clear_db(); 
        
        port = find_available_port();

        server = new DispatchServer();
        server->set_api_key(api_key);
        server->start(port, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        client = new httplib::Client("localhost", port);
        client->set_connection_timeout(30);
        wait_for_server_ready();

        admin_headers = {
            {"Authorization", "some_token"},
            {"X-API-Key", api_key}
        };
    }

    static void TearDownTestSuite() {
        server->stop();
        delete server;
        delete client;
        std::remove(persistent_storage_file.c_str());
        PERSISTENT_STORAGE_FILE = "dispatch_server_state.json";
    }

    void SetUp() override {
        clear_db();
    }

    void TearDown() override {
        // Per-test teardown, if any, goes here.
    }

    static void wait_for_server_ready() {
        int retries = 20;
        while (retries-- > 0) {
            auto res = client->Get("/");
            if (res && res->status == 200) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        FAIL() << "Server did not become ready in time.";
    }
};

#endif // DISPATCH_SERVER_TEST_COMMON_H
