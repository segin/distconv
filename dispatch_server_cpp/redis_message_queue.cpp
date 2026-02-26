#include "redis_message_queue.h"
#include <iostream>
#include <uuid/uuid.h>
#include <chrono>

namespace distconv {

// Producer
RedisMessageQueueProducer::RedisMessageQueueProducer(std::shared_ptr<sw::redis::Redis> redis)
    : redis_(std::move(redis)) {}

bool RedisMessageQueueProducer::publish(const std::string& topic, const std::string& payload) {
    try {
        std::vector<std::pair<std::string, std::string>> attrs = {{"payload", payload}};
        redis_->xadd(topic, "*", attrs.begin(), attrs.end());
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Redis publish error: " << e.what() << std::endl;
        return false;
    }
}

// Consumer
RedisMessageQueueConsumer::RedisMessageQueueConsumer(std::shared_ptr<sw::redis::Redis> redis, std::string group_id, std::string consumer_name)
    : redis_(std::move(redis)), group_id_(std::move(group_id)), consumer_name_(std::move(consumer_name)) {
    if (consumer_name_.empty()) {
        uuid_t uuid;
        uuid_generate(uuid);
        char uuid_str[37];
        uuid_unparse(uuid, uuid_str);
        consumer_name_ = uuid_str;
    }
}

RedisMessageQueueConsumer::~RedisMessageQueueConsumer() {
    running_ = false;
    {
        std::lock_guard<std::mutex> lock(threads_mutex_);
        for (auto& t : poll_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
}

void RedisMessageQueueConsumer::subscribe(const std::string& topic, std::function<void(const Message&)> callback) {
    // Create group if not exists
    try {
        // MKSTREAM option creates stream if not exists via the create group command if supported or implicitly
        // redis-plus-plus xgroup_create creates the group. 
        // We use "$" to start consuming from now, or "0" for all. 
        // Usually for a work queue we want stream to exist.
        // If stream doesn't exist, xgroup_create might fail without MKSTREAM option (depending on redis version).
        // redis-plus-plus api might assume stream exists.
        // Let's try to ensure stream exists with xadd if needed, or rely on MKSTREAM arg if available.
        // The c++ library might not expose MKSTREAM flag easily in older versions?
        // Let's check if we can pass it.
        // Function signature: void xgroup_create(const StringView &key, const StringView &group, const StringView &id = "$", bool mkstream = false);
        redis_->xgroup_create(topic, group_id_, "$", true);
    } catch (const sw::redis::Error& e) {
        // Ignore if group already exists (BUSYGROUP)
        std::string msg = e.what();
        if (msg.find("BUSYGROUP") == std::string::npos) {
             std::cerr << "Warning creating group: " << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
         std::cerr << "Error creating group: " << e.what() << std::endl;
    }

    std::lock_guard<std::mutex> lock(threads_mutex_);
    poll_threads_.emplace_back(&RedisMessageQueueConsumer::pollLoop, this, topic, callback);
}

void RedisMessageQueueConsumer::pollLoop(std::string topic, std::function<void(const Message&)> callback) {
    while (running_) {
        try {
            // Prepare container for results
            // Type: vector<pair<stream_name, vector<pair<message_id, vector<pair<field, value>>>>>>
            using ItemStream = std::pair<std::string, std::vector<std::pair<std::string, std::string>>>;
            using ItemBatch = std::pair<std::string, std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>>>;
            std::vector<ItemBatch> result;

            // Read new messages
            // Block 2 seconds. count 1.
            redis_->xreadgroup(group_id_, consumer_name_, {topic}, {">"}, 1, 2000, std::back_inserter(result));
            
            for (const auto& topic_batch : result) {
                if (topic_batch.first == topic) {
                    for (const auto& message_entry : topic_batch.second) {
                        std::string id = message_entry.first;
                        std::string payload;
                        // message_entry.second is vector<pair<string, string>> (fields)
                        for (const auto& field : message_entry.second) {
                            if (field.first == "payload") {
                                payload = field.second;
                                break;
                            }
                        }
                        
                        if (!payload.empty()) {
                             callback({topic, payload, id});
                        }
                    }
                }
            }
        } catch (const sw::redis::TimeoutError& e) {
            // Timeout is expected
            continue;
        } catch (const sw::redis::Error& e) {
             std::cerr << "Redis poll error: " << e.what() << std::endl;
             std::this_thread::sleep_for(std::chrono::seconds(1));
        } catch (const std::exception& e) {
            std::cerr << "Redis poll exception: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

bool RedisMessageQueueConsumer::ack(const Message& message) {
    try {
        redis_->xack(message.topic, group_id_, {message.id});
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Redis ack error: " << e.what() << std::endl;
        return false;
    }
}

bool RedisMessageQueueConsumer::nack(const Message& message) {
    try {
        // Move to Dead Letter Queue (DLQ)
        std::string dlq_topic = message.topic + "-dlq";
        std::vector<std::pair<std::string, std::string>> attrs = {
            {"payload", message.payload},
            {"original_id", message.id},
            {"reason", "nacked"}
        };
        redis_->xadd(dlq_topic, "*", attrs.begin(), attrs.end());
        
        // Acknowledge the original message so it's removed from the pending list
        redis_->xack(message.topic, group_id_, {message.id});
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Redis nack error: " << e.what() << std::endl;
        return false;
    }
}

// Factory
RedisMessageQueueFactory::RedisMessageQueueFactory(const std::string& redis_uri) {
    redis_ = std::make_shared<sw::redis::Redis>(redis_uri);
}

std::unique_ptr<MessageQueueProducer> RedisMessageQueueFactory::createProducer() {
    return std::make_unique<RedisMessageQueueProducer>(redis_);
}

std::unique_ptr<MessageQueueConsumer> RedisMessageQueueFactory::createConsumer(const std::string& group_id) {
    return std::make_unique<RedisMessageQueueConsumer>(redis_, group_id, "");
}

} // namespace distconv
