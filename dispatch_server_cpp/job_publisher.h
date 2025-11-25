#pragma once

#include "message_queue.h"
#include <memory>
#include <string>

namespace distconv {

class JobPublisher {
public:
    JobPublisher(std::shared_ptr<MessageQueueProducer> producer);

    // Publishes a job to the "jobs" topic.
    // The payload is expected to be a JSON string representing the job.
    bool publishJob(const std::string& job_json);

private:
    std::shared_ptr<MessageQueueProducer> producer_;
};

} // namespace distconv
