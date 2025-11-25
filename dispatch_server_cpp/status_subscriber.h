#pragma once

#include "message_queue.h"
#include <memory>
#include <string>
#include <functional>

namespace distconv {

class StatusSubscriber {
public:
    StatusSubscriber(std::shared_ptr<MessageQueueConsumer> consumer);

    // Subscribes to the "status" topic.
    void subscribeToStatusUpdates(std::function<void(const std::string&)> callback);

private:
    std::shared_ptr<MessageQueueConsumer> consumer_;
};

} // namespace distconv
