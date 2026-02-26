#include "../backoff_timer.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

using namespace distconv::TranscodingEngine;

void test_initial_state() {
    std::cout << "Testing initial state..." << std::endl;
    BackoffTimer timer(std::chrono::milliseconds(10), std::chrono::milliseconds(100));
    assert(timer.get_current_delay().count() == 10);
    std::cout << "  Passed." << std::endl;
}

void test_backoff_progression() {
    std::cout << "Testing backoff progression..." << std::endl;
    // Use slightly larger values to avoid precision issues with integer math if any
    BackoffTimer timer(std::chrono::milliseconds(10), std::chrono::milliseconds(100));

    // Initial: 10
    assert(timer.get_current_delay().count() == 10);

    // Sleep (consumes 10ms, computes next)
    timer.sleep();
    long long delay1 = timer.get_current_delay().count();

    // Expect roughly 20 (10 * 2). Jitter +/- 10% of 20 = +/- 2. Range: 18-22.
    std::cout << "  After 1st sleep: " << delay1 << "ms (expected 18-22)" << std::endl;
    assert(delay1 >= 18 && delay1 <= 22);

    // Sleep (consumes ~20ms, computes next)
    timer.sleep();
    long long delay2 = timer.get_current_delay().count();

    // Expect roughly 40 (20 * 2). Jitter +/- 10% of 40 = +/- 4. Range: 36-44.
    // Note: It uses the *actual* previous delay for calculation.
    // if delay1 was 20, next is 40 +/- 4.
    // if delay1 was 18, next is 36 +/- 3. Range 33-39.
    // if delay1 was 22, next is 44 +/- 4. Range 40-48.
    // Overall range: 33-48.
    std::cout << "  After 2nd sleep: " << delay2 << "ms (expected > " << delay1 << ")" << std::endl;
    assert(delay2 > delay1);
    assert(delay2 < 100);

    std::cout << "  Passed." << std::endl;
}

void test_max_delay_cap() {
    std::cout << "Testing max delay cap..." << std::endl;
    BackoffTimer timer(std::chrono::milliseconds(1), std::chrono::milliseconds(10));

    // Loop enough times to hit max
    for (int i = 0; i < 10; ++i) {
        timer.sleep();
    }

    long long current = timer.get_current_delay().count();
    // Should be capped at 10
    std::cout << "  After 10 sleeps: " << current << "ms (expected 10)" << std::endl;
    assert(current == 10);

    std::cout << "  Passed." << std::endl;
}

void test_reset() {
    std::cout << "Testing reset..." << std::endl;
    BackoffTimer timer(std::chrono::milliseconds(10), std::chrono::milliseconds(100));

    timer.sleep();
    timer.sleep();
    assert(timer.get_current_delay().count() > 10);

    timer.reset();
    assert(timer.get_current_delay().count() == 10);

    std::cout << "  Passed." << std::endl;
}

int main() {
    test_initial_state();
    test_backoff_progression();
    test_max_delay_cap();
    test_reset();

    std::cout << "All BackoffTimer tests passed!" << std::endl;
    return 0;
}
