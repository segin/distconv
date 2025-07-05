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
    jobs_db = nlohmann::json::object();
    engines_db = nlohmann::json::object();
    // Also clear the persistent storage file
    std::remove(PERSISTENT_STORAGE_FILE.c_str());
}

// Test fixture for API tests
class ApiTest : public ::testing::Test {
protected:
    DispatchServer *server;
    httplib::Client *client;
    int port = 8080; // Default port for tests
    std::string api_key = "test_api_key";

    void SetUp() override {
        clear_db();
        server = new DispatchServer(api_key);
        // Start the server in a non-blocking way
        server->start(port, false);
        client = new httplib::Client("localhost", port);
        client->set_connection_timeout(5);
    }

    void TearDown() override {
        delete client;
        server->stop();
        delete server;
    }
};

#endif // DISPATCH_SERVER_TEST_COMMON_H
