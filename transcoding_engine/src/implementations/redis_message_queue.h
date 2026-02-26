#pragma once

#include "interfaces/message_queue.h"
#include <sw/redis++/redis++.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>

namespace distconv {

class RedisMessageQueueProducer : public MessageQueueProducer {
public:
    RedisMessageQueueProducer(std::shared_ptr<sw::redis::Redis> redis);
    bool publish(const std::string& topic, const std::string& payload) override;

private:
    std::shared_ptr<sw::redis::Redis> redis_;
};

class RedisMessageQueueConsumer : public MessageQueueConsumer {
public:
    RedisMessageQueueConsumer(std::shared_ptr<sw::redis::Redis> redis, std::string group_id, std::string consumer_name);
    ~RedisMessageQueueConsumer();

    void subscribe(const std::string& topic, std::function<void(const Message&)> callback) override;
    bool ack(const Message& message) override;
    bool nack(const Message& message) override;

private:
    void pollLoop(std::string topic, std::function<void(const Message&)> callback);

    std::shared_ptr<sw::redis::Redis> redis_;
    std::string group_id_;
    std::string consumer_name_;
    std::atomic<bool> running_{true};
    std::vector<std::thread> poll_threads_;
    std::mutex threads_mutex_;
};

class RedisMessageQueueFactory : public MessageQueueFactory {
public:
    RedisMessageQueueFactory(const std::string& redis_uri);
    std::unique_ptr<MessageQueueProducer> createProducer() override;
    std::unique_ptr<MessageQueueConsumer> createConsumer(const std::string& group_id) override;

private:
    std::shared_ptr<sw::redis::Redis> redis_;
};

} // namespace distconv
