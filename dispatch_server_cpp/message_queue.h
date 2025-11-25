#pragma once

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <future>

namespace distconv {

struct Message {
    std::string topic;
    std::string payload;
    std::string id;
};

class MessageQueueProducer {
public:
    virtual ~MessageQueueProducer() = default;

    // Publishes a message to a topic. Returns true if successful.
    virtual bool publish(const std::string& topic, const std::string& payload) = 0;
};

class MessageQueueConsumer {
public:
    virtual ~MessageQueueConsumer() = default;

    // Subscribes to a topic. The callback is invoked when a message is received.
    virtual void subscribe(const std::string& topic, std::function<void(const Message&)> callback) = 0;

    // Acknowledges a message has been processed.
    virtual bool ack(const std::string& message_id) = 0;

    // Negatively acknowledges a message (optional).
    virtual bool nack(const std::string& message_id) = 0;
};

// Factory interface for creating producers and consumers
class MessageQueueFactory {
public:
    virtual ~MessageQueueFactory() = default;
    virtual std::unique_ptr<MessageQueueProducer> createProducer() = 0;
    virtual std::unique_ptr<MessageQueueConsumer> createConsumer(const std::string& group_id) = 0;
};

} // namespace distconv
