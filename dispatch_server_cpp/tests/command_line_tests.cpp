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

    void start_server_and_wait(const std::string& api_key = "") {
        // Create argv for run_dispatch_server
        std::vector<std::string> args_storage;
        args_storage.push_back("dispatch_server_app");
        if (!api_key.empty()) {
            args_storage.push_back("--api-key");
            args_storage.push_back(api_key);
        }

        std::vector<char*> argv_c_str;
        for (const auto& s : args_storage) {
            argv_c_str.push_back(const_cast<char*>(s.c_str()));
        }

        int argc = argv_c_str.size();
        char** argv = argv_c_str.data();

        server_instance = new DispatchServer(); // Create the server instance
        run_dispatch_server(argc, argv, server_instance); // Pass the instance to run_dispatch_server
        
        // Give the server a moment to start its thread
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

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
    start_server_and_wait();
    // TearDown will handle stopping and cleanup
}

TEST_F(CommandLineTest, ServerParsesApiKeyArgument) {
    // This test verifies that the server correctly parses the --api-key argument.
    // We start the server with a specific API key and then try to access an endpoint
    // with and without that API key.
    const std::string test_api_key = "my_test_api_key";
    start_server_and_wait(test_api_key);

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

TEST_F(CommandLineTest, ServerHandlesApiKeyWithoutValueGracefully) {
    // This test verifies that the server handles --api-key without a value gracefully.
    int argc = 2;
    char* argv[] = {(char*)"dispatch_server_app", (char*)"--api-key"};

    // Start the server in a separate thread
    server_instance = new DispatchServer();
    std::thread server_thread([&]() {
        run_dispatch_server(argc, argv, server_instance);
    });

    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client = new httplib::Client("localhost", 8080);
    client->set_connection_timeout(10);

    // Try to access an endpoint. It should be unauthorized as no API key is set.
    auto res = client->Get("/jobs/");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 401);
}

TEST_F(CommandLineTest, ServerIgnoresUnknownArguments) {
    // This test verifies that the server ignores unknown command-line arguments
    // and does not crash or behave unexpectedly.
    const std::string test_api_key = "my_test_api_key";
    // We need to manually construct argv for run_dispatch_server
    std::vector<std::string> args_storage;
    args_storage.push_back("dispatch_server_app");
    args_storage.push_back("--api-key");
    args_storage.push_back(test_api_key);
    args_storage.push_back("--unknown-arg");
    args_storage.push_back("some_value");

    std::vector<char*> argv_c_str;
    for (const auto& s : args_storage) {
        argv_c_str.push_back(const_cast<char*>(s.c_str()));
    }

    int argc = argv_c_str.size();
    char** argv = argv_c_str.data();

    server_instance = new DispatchServer(); // Create the server instance
    run_dispatch_server(argc, argv, server_instance);

    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client = new httplib::Client("localhost", 8080);
    client->set_connection_timeout(10);

    // Try to access an endpoint with the correct API key to verify server is running
    httplib::Headers headers = {
        {"X-API-Key", test_api_key}
    };
    auto res_with_api_key = client->Get("/jobs/");
    ASSERT_TRUE(res_with_api_key != nullptr);
    ASSERT_EQ(res_with_api_key->status, 200);
}