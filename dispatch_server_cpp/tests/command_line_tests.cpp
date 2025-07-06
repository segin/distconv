#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h"

// Test fixture for command-line argument tests
class CommandLineTest : public ::testing::Test {
protected:
    DispatchServer *server_instance;
    httplib::Client *client;
    int port = 8080; // Default port for command line tests

    void SetUp() override {
        server_instance = nullptr;
        client = nullptr;
    }

    void TearDown() override {
        if (server_instance) {
            server_instance->stop();
            delete server_instance;
            server_instance = nullptr;
        }
        if (client) {
            delete client;
            client = nullptr;
        }
    }

    void start_server_in_thread(const std::string& api_key = "") {
        server_instance = new DispatchServer(api_key);
        server_instance->start(port, false); // Start in non-blocking mode
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give server time to start
        client = new httplib::Client("localhost", port);
        client->set_connection_timeout(10);
        // Wait for server to be ready
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

TEST_F(CommandLineTest, ServerStartsAndStopsCleanly) {
    // This test verifies that the server can be started and stopped cleanly
    // without crashing or leaking resources.
    start_server_in_thread();
    // TearDown will handle stopping and cleanup
}

TEST_F(CommandLineTest, ServerParsesApiKeyArgument) {
    // This test verifies that the server correctly parses the --api-key argument.
    // We start the server with a specific API key and then try to access an endpoint
    // with and without that API key.
    const std::string test_api_key = "my_test_api_key";
    start_server_in_thread(test_api_key);

    // Try to access an endpoint without the API key
    auto res_no_api_key = client->Get("/jobs/");
    ASSERT_TRUE(res_no_api_key != nullptr);
    ASSERT_EQ(res_no_api_key->status, 401);

    // Try to access the endpoint with the correct API key
    httplib::Headers headers = {
        {"X-API-Key", test_api_key}
    };
    auto res_with_api_key = client->Get("/jobs/", headers);
    ASSERT_TRUE(res_with_api_key != nullptr);
    ASSERT_EQ(res_with_api_key->status, 200);
}
