#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <condition_variable>
#include <mutex>

#include "test_common.h" // Include for ApiTest static members

using namespace distconv::DispatchServer;

// Define static members outside the class


std::mutex m;
std::condition_variable cv;
bool tests_completed = false;

void watchdog_timer(int timeout_seconds) {
    std::unique_lock<std::mutex> lock(m);
    if (cv.wait_for(lock, std::chrono::seconds(timeout_seconds), []{ return tests_completed; })) {
        // Tests completed in time.
    } else {
        std::cerr << "!!! TEST TIMEOUT (" << timeout_seconds << "s) !!!" << std::endl;
        std::exit(1);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    const int timeout_seconds = 300; // 5 minutes
    std::thread watchdog(watchdog_timer, timeout_seconds);

    int result = RUN_ALL_TESTS();

    {
        std::lock_guard<std::mutex> lock(m);
        tests_completed = true;
    }
    cv.notify_one();

    watchdog.join();

    return result;
}
