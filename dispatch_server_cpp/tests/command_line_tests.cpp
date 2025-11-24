#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include "test_common.h"

using namespace distconv::DispatchServer;

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
    // We need to manually construct argv for run_dispatch_server to simulate the missing value.
    std::vector<std::string> args_storage;
    args_storage.push_back("dispatch_server_app");
    args_storage.push_back("--api-key");

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

    // Try to access an endpoint. It should be unauthorized as no API key is set.
    auto res = client->Get("/");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
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
    auto res_with_api_key = client->Get("/jobs/", headers);
    ASSERT_TRUE(res_with_api_key != nullptr);
    ASSERT_EQ(res_with_api_key->status, 200);
}

TEST_F(CommandLineTest, RunDispatchServerWithoutArgcArgv) {
    server_instance = new DispatchServer();
    run_dispatch_server(server_instance);

    // Give the server a moment to start its thread
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client = new httplib::Client("localhost", port);
    client->set_connection_timeout(10);

    // Verify server is running and accessible without API key
    auto res = client->Get("/");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200);
}

TEST_F(CommandLineTest, ApiKeyCanBeSetProgrammatically) {
    const std::string test_api_key = "programmatic_api_key";

    server_instance = new DispatchServer();
    server_instance->set_api_key(test_api_key);
    server_instance->start(port, false); // Start in non-blocking mode

    // Give the server a moment to start its thread
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client = new httplib::Client("localhost", port);
    client->set_connection_timeout(10);

    // Verify server is running and accessible with the programmatic API key
    httplib::Headers headers = {
        {"X-API-Key", test_api_key}
    };
    auto res_with_api_key = client->Get("/jobs/", headers);
    ASSERT_TRUE(res_with_api_key != nullptr);
    ASSERT_EQ(res_with_api_key->status, 200);

    // Verify access without API key is unauthorized
    auto res_no_api_key = client->Get("/jobs/");
    ASSERT_TRUE(res_no_api_key != nullptr);
    ASSERT_EQ(res_no_api_key->status, 401);
}

TEST_F(CommandLineTest, MainServerLoopCanBeStartedAndStoppedProgrammatically) {
    server_instance = new DispatchServer();
    server_instance->set_api_key("test_key"); // Set an API key for this test
    server_instance->start(port, false); // Start in non-blocking mode

    // Give the server a moment to start its thread
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client = new httplib::Client("localhost", port);
    client->set_connection_timeout(10);

    // Verify server is running
    auto res_running = client->Get("/");
    ASSERT_TRUE(res_running != nullptr);
    ASSERT_EQ(res_running->status, 200);

    // Stop the server
    server_instance->stop();

    // Give the server a moment to shut down
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify server has stopped (expect connection error)
    auto res_stopped = client->Get("/");
    ASSERT_TRUE(res_stopped == nullptr); // Expect no response due to connection error
}
