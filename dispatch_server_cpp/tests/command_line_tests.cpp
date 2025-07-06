#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h"

// Test fixture for command-line argument tests
class CommandLineTest : public ::testing::Test {
protected:
    DispatchServer *server;

    void SetUp() override {
        // Initialize server here, but don't start listening yet
        server = new DispatchServer("test_api_key");
    }

    void TearDown() override {
        // Stop the server and clean up
        server->stop();
        delete server;
    }
};

TEST_F(CommandLineTest, ServerStartsAndStopsCleanly) {
    // This test verifies that the server can be started and stopped cleanly
    // without crashing or leaking resources.
    // We start the server in a non-blocking mode and then immediately stop it.
    server->start(8080, false);
    // Give the server a moment to start its thread before stopping
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // The TearDown method will call server->stop() and delete server
}