#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../dispatch_server_core.h"
#include "../message_queue.h"
#include "../memory_message_queue.h"
#include "../repositories.h"

using namespace distconv;
// Do NOT use namespace distconv::DispatchServer due to collision with class name
// using namespace distconv::DispatchServer;

class MessageQueueIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(MessageQueueIntegrationTest, JobSubmissionPublishesToQueue) {
    // Setup InMemory repositories
    auto job_repo = std::make_shared<distconv::DispatchServer::InMemoryJobRepository>();
    auto engine_repo = std::make_shared<distconv::DispatchServer::InMemoryEngineRepository>();

    // Setup MemoryMessageQueue
    auto mq_factory = std::make_unique<MemoryMessageQueueFactory>();

    // Create Consumer to spy on the queue
    auto consumer = mq_factory->createConsumer("test-group");
    std::vector<std::string> received_messages;
    consumer->subscribe("jobs", [&](const Message& msg) {
        received_messages.push_back(msg.payload);
    });

    // Setup Server with injected dependencies
    distconv::DispatchServer::DispatchServer server(job_repo, engine_repo, std::move(mq_factory), "test-api-key");
    auto* svr = server.getServer();

    // Simulate Job Submission
    httplib::Client cli("localhost", 8080); // We won't actually connect, we'll use the server object directly via internal handler logic if possible, or we need to start it.
    // Starting the server is asynchronous in tests usually, or we can use `svr->dispatch_request`.

    // httplib::Server::dispatch_request is private? No, let's check.
    // It seems we can't easily call dispatch_request from outside without hacking httplib.

    // Let's use a fixed port for testing, hoping it's free. 8089.
    // Try-catch block to handle port binding failures gracefully or diagnose crash
    try {
        server.start(8089, false);
    } catch (...) {
        FAIL() << "Failed to start server on port 8089";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait for start

    httplib::Client client("localhost", 8089);
    client.set_read_timeout(5); // Increased timeout

    nlohmann::json job_req;
    job_req["source_url"] = "http://example.com/video.mp4";
    job_req["target_codec"] = "h264";

    auto res = client.Post("/jobs/", {{"X-API-KEY", "test-api-key"}}, job_req.dump(), "application/json");

    if (!res) {
        // If request failed, stop server and fail test
        server.stop();
        FAIL() << "HTTP Request failed. Error: " << res.error();
    }

    EXPECT_EQ(res->status, 201);

    // Check if message was received by consumer.
    // We assume MemoryMessageQueue requires explicit polling in this test setup.
    auto* mem_consumer = dynamic_cast<MemoryMessageQueueConsumer*>(consumer.get());
    ASSERT_NE(mem_consumer, nullptr);
    mem_consumer->poll("jobs");

    ASSERT_FALSE(received_messages.empty());
    auto json_msg = nlohmann::json::parse(received_messages[0]);
    EXPECT_EQ(json_msg["source_url"], "http://example.com/video.mp4");

    server.stop();
}
