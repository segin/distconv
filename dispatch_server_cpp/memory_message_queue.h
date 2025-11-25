#pragma once

#include "message_queue.h"
#include <queue>
#include <mutex>
#include <unordered_map>
#include <iostream>

namespace distconv {

// A simple in-memory message queue for testing and development.
// Not for production use across multiple processes.
class MemoryMessageQueueProducer : public MessageQueueProducer {
public:
    MemoryMessageQueueProducer(std::shared_ptr<std::unordered_map<std::string, std::vector<Message>>> storage, std::shared_ptr<std::mutex> mutex)
        : storage_(storage), mutex_(mutex) {}

    bool publish(const std::string& topic, const std::string& payload) override {
        std::lock_guard<std::mutex> lock(*mutex_);
        (*storage_)[topic].push_back({topic, payload, std::to_string(message_id_counter_++)});
        return true;
    }

private:
    std::shared_ptr<std::unordered_map<std::string, std::vector<Message>>> storage_;
    std::shared_ptr<std::mutex> mutex_;
    int message_id_counter_ = 0;
};

class MemoryMessageQueueConsumer : public MessageQueueConsumer {
public:
    MemoryMessageQueueConsumer(std::shared_ptr<std::unordered_map<std::string, std::vector<Message>>> storage, std::shared_ptr<std::mutex> mutex)
        : storage_(storage), mutex_(mutex) {}

    void subscribe(const std::string& topic, std::function<void(const Message&)> callback) override {
        // In a real implementation this would be a loop or event driven.
        // Here we just check what's currently in the storage for the topic.
        // This is a naive implementation for testing integration.
        std::lock_guard<std::mutex> lock(*mutex_);
        if (storage_->find(topic) != storage_->end()) {
            for (const auto& msg : (*storage_)[topic]) {
                callback(msg);
            }
        }
        // In a real system we would register the callback for future messages.
        callbacks_[topic].push_back(callback);
    }

    // Simulate receiving new messages (manual trigger for testing)
    void poll(const std::string& topic) {
        std::lock_guard<std::mutex> lock(*mutex_);
        if (storage_->find(topic) != storage_->end() && callbacks_.find(topic) != callbacks_.end()) {
             auto& msgs = (*storage_)[topic];

             // Track offset per topic to avoid replay
             size_t start_index = offsets_[topic];

             for(auto& cb : callbacks_[topic]) {
                 for (size_t i = start_index; i < msgs.size(); ++i) {
                     cb(msgs[i]);
                 }
             }

             offsets_[topic] = msgs.size();
        }
    }

    bool ack(const std::string& message_id) override {
        return true;
    }

    bool nack(const std::string& message_id) override {
        return true;
    }

private:
    std::shared_ptr<std::unordered_map<std::string, std::vector<Message>>> storage_;
    std::shared_ptr<std::mutex> mutex_;
    std::unordered_map<std::string, std::vector<std::function<void(const Message&)>>> callbacks_;
    std::unordered_map<std::string, size_t> offsets_;
};

class MemoryMessageQueueFactory : public MessageQueueFactory {
public:
    MemoryMessageQueueFactory()
        : storage_(std::make_shared<std::unordered_map<std::string, std::vector<Message>>>()),
          mutex_(std::make_shared<std::mutex>()) {}

    std::unique_ptr<MessageQueueProducer> createProducer() override {
        return std::make_unique<MemoryMessageQueueProducer>(storage_, mutex_);
    }

    std::unique_ptr<MessageQueueConsumer> createConsumer(const std::string& group_id) override {
        return std::make_unique<MemoryMessageQueueConsumer>(storage_, mutex_);
    }

private:
    std::shared_ptr<std::unordered_map<std::string, std::vector<Message>>> storage_;
    std::shared_ptr<std::mutex> mutex_;
};

} // namespace distconv
