#pragma once

#include "../message_queue.h"
#include <trompeloeil.hpp>

namespace distconv {
namespace tests {

class MockMessageQueueProducer : public MessageQueueProducer {
public:
    MAKE_MOCK2(publish, bool(const std::string&, const std::string&), override);
};

class MockMessageQueueConsumer : public MessageQueueConsumer {
public:
    MAKE_MOCK2(subscribe, void(const std::string&, std::function<void(const Message&)>), override);
    MAKE_MOCK1(ack, bool(const std::string&), override);
    MAKE_MOCK1(nack, bool(const std::string&), override);
};

class MockMessageQueueFactory : public MessageQueueFactory {
public:
    MAKE_MOCK0(createProducer, std::unique_ptr<MessageQueueProducer>(), override);
    MAKE_MOCK1(createConsumer, std::unique_ptr<MessageQueueConsumer>(const std::string&), override);
};

} // namespace tests
} // namespace distconv
