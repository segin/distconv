#include "status_subscriber.h"

namespace distconv {

StatusSubscriber::StatusSubscriber(std::shared_ptr<MessageQueueConsumer> consumer)
    : consumer_(std::move(consumer)) {}

void StatusSubscriber::subscribeToStatusUpdates(std::function<void(const std::string&)> callback) {
    if (!consumer_) {
        return;
    }

    consumer_->subscribe("status", [callback](const Message& msg) {
        callback(msg.payload);
    });
}

} // namespace distconv
