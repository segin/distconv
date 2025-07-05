#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream> // Required for std::ofstream
#include <cstdio> // Required for std::remove
#include <thread>
#include <chrono>

#include "test_common.h" // For ApiTest fixture and clear_db()

// Test fixture for API tests
class ApiTest : public ::testing::Test {
protected:
    httplib::Client cli;
    ApiTest() : cli("localhost", 8080) {}

    void SetUp() override {
        // Clear the database before each test
        clear_db();
    }
};
