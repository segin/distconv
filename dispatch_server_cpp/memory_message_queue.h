#pragma once

#include "message_queue.h"
#include <queue>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <iostream>

namespace distconv {

struct TopicQueue {
    std::deque<Message> messages;
    uint64_t base_offset = 0;
};

// A simple in-memory message queue for testing and development.
// Not for production use across multiple processes.
class MemoryMessageQueueProducer : public MessageQueueProducer {
public:
    MemoryMessageQueueProducer(std::shared_ptr<std::unordered_map<std::string, TopicQueue>> storage, std::shared_ptr<std::mutex> mutex)
        : storage_(storage), mutex_(mutex) {}

    bool publish(const std::string& topic, const std::string& payload) override {
        std::lock_guard<std::mutex> lock(*mutex_);
        auto& queue = (*storage_)[topic];
        queue.messages.push_back({topic, payload, std::to_string(message_id_counter_++)});

        // Limit queue size to prevent unbounded memory growth
        // 1000 messages * ~1KB = ~1MB per topic
        const size_t MAX_QUEUE_SIZE = 1000;
        while (queue.messages.size() > MAX_QUEUE_SIZE) {
            queue.messages.pop_front();
            queue.base_offset++;
        }
        return true;
    }

private:
    std::shared_ptr<std::unordered_map<std::string, TopicQueue>> storage_;
    std::shared_ptr<std::mutex> mutex_;
    int message_id_counter_ = 0;
};

class MemoryMessageQueueConsumer : public MessageQueueConsumer {
public:
    MemoryMessageQueueConsumer(std::shared_ptr<std::unordered_map<std::string, TopicQueue>> storage, std::shared_ptr<std::mutex> mutex)
        : storage_(storage), mutex_(mutex) {}

    void subscribe(const std::string& topic, std::function<void(const Message&)> callback) override {
        // In a real implementation this would be a loop or event driven.
        // Here we just check what's currently in the storage for the topic.
        // This is a naive implementation for testing integration.
        std::lock_guard<std::mutex> lock(*mutex_);
        if (storage_->find(topic) != storage_->end()) {
            const auto& queue = (*storage_)[topic];
            for (const auto& msg : queue.messages) {
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
             auto& queue = (*storage_)[topic];

             // Track offset per topic to avoid replay
             uint64_t current_offset = offsets_[topic];

             // If consumer is behind the window (messages lost due to cleanup), skip to the beginning
             if (current_offset < queue.base_offset) {
                 current_offset = queue.base_offset;
             }

             // Calculate index in the deque
             size_t start_index = current_offset - queue.base_offset;

             if (start_index < queue.messages.size()) {
                 for(auto& cb : callbacks_[topic]) {
                     for (size_t i = start_index; i < queue.messages.size(); ++i) {
                         cb(queue.messages[i]);
                     }
                 }
                 // Update offset to the end
                 offsets_[topic] = queue.base_offset + queue.messages.size();
             }
        }
    }

    bool ack(const std::string& message_id) override {
        return true;
    }

    bool nack(const std::string& message_id) override {
        return true;
    }

private:
    std::shared_ptr<std::unordered_map<std::string, TopicQueue>> storage_;
    std::shared_ptr<std::mutex> mutex_;
    std::unordered_map<std::string, std::vector<std::function<void(const Message&)>>> callbacks_;
    std::unordered_map<std::string, uint64_t> offsets_;
};

class MemoryMessageQueueFactory : public MessageQueueFactory {
public:
    MemoryMessageQueueFactory()
        : storage_(std::make_shared<std::unordered_map<std::string, TopicQueue>>()),
          mutex_(std::make_shared<std::mutex>()) {}

    std::unique_ptr<MessageQueueProducer> createProducer() override {
        return std::make_unique<MemoryMessageQueueProducer>(storage_, mutex_);
    }

    std::unique_ptr<MessageQueueConsumer> createConsumer(const std::string& group_id) override {
        return std::make_unique<MemoryMessageQueueConsumer>(storage_, mutex_);
    }

private:
    std::shared_ptr<std::unordered_map<std::string, TopicQueue>> storage_;
    std::shared_ptr<std::mutex> mutex_;
};

} // namespace distconv
