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
    nlohmann::json job_req;
    job_req["source_url"] = "http://example.com/video.mp4";
    job_req["target_codec"] = "h264";

    httplib::Request req;
    req.method = "POST";
    req.path = "/jobs/";
    req.headers.emplace("X-API-KEY", "test-api-key");
    req.body = job_req.dump();
    req.set_header("Content-Type", "application/json");

    httplib::Response res;
    httplib::detail::BufferStream strm;

    // Use internal routing method exposed for testing
    svr->routing(req, res, strm);

    EXPECT_EQ(res.status, 201);

    // Check if message was received by consumer
    // We need to trigger the consumer poll because MemoryMessageQueue might be passive or active.
    // In my implementation, subscribe just registers. publish pushes to storage.
    // But subscribe implementation in MemoryMessageQueue:
    // "In a real implementation this would be a loop or event driven. Here we just check what's currently in the storage"
    // So we need to call subscribe AFTER publish or use a polling mechanism.

    // Wait, my MemoryMessageQueueConsumer::subscribe implementation:
    // checks storage immediately.
    // registers callback.
    // BUT does it invoke callback on new messages?
    // MemoryMessageQueueProducer::publish just pushes to storage vector.
    // It DOES NOT invoke callbacks.
    // I added a `poll` method to MemoryMessageQueueConsumer in my implementation.
    // But `MessageQueueConsumer` interface doesn't have `poll`.
    // So I need to cast it or change the test strategy.

    // Actually, I can cast it since I know the concrete type in the test.
    // But wait, `createConsumer` returns `unique_ptr<MessageQueueConsumer>`.
    // I lost the concrete type pointer unless I kept a reference or casted it.
    // But `consumer` variable is `unique_ptr<MessageQueueConsumer>`.

    // So I need to cast:
    auto* mem_consumer = dynamic_cast<MemoryMessageQueueConsumer*>(consumer.get());
    ASSERT_NE(mem_consumer, nullptr);
    mem_consumer->poll("jobs");

    ASSERT_FALSE(received_messages.empty());
    auto json_msg = nlohmann::json::parse(received_messages[0]);
    EXPECT_EQ(json_msg["source_url"], "http://example.com/video.mp4");

    server.stop();
}
