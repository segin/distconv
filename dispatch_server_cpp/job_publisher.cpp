#include "job_publisher.h"

namespace distconv {

JobPublisher::JobPublisher(std::shared_ptr<MessageQueueProducer> producer)
    : producer_(std::move(producer)) {}

bool JobPublisher::publishJob(const std::string& job_json) {
    if (!producer_) {
        return false;
    }
    return producer_->publish("jobs", job_json);
}

} // namespace distconv
