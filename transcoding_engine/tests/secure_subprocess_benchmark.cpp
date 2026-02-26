#include "../src/implementations/secure_subprocess.h"
#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <chrono>

using namespace distconv::TranscodingEngine;

int main() {
    SecureSubprocess subprocess;
    // Command that sleeps for 1ms
    std::vector<std::string> command = {"sleep", "0.001"};

    const int iterations = 100;
    std::vector<double> durations;

    std::cout << "Running benchmark with " << iterations << " iterations..." << std::endl;

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        subprocess.run(command, "", 1);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> duration = end - start;
        durations.push_back(duration.count());
    }

    double sum = std::accumulate(durations.begin(), durations.end(), 0.0);
    double avg = sum / iterations;

    double min = *std::min_element(durations.begin(), durations.end());
    double max = *std::max_element(durations.begin(), durations.end());

    std::cout << "Results:" << std::endl;
    std::cout << "  Average duration: " << avg << " ms" << std::endl;
    std::cout << "  Min duration:     " << min << " ms" << std::endl;
    std::cout << "  Max duration:     " << max << " ms" << std::endl;

    // Test timeout
    std::cout << "\nTesting timeout functionality..." << std::endl;
    // Use sh -c to close stdout/stderr so read_from_pipe returns immediately
    // sleep 2 will keep running to test the timeout logic in wait_with_timeout
    command = {"sh", "-c", "exec sleep 2 >&- 2>&-"};

    auto start = std::chrono::high_resolution_clock::now();
    SubprocessResult result = subprocess.run(command, "", 1);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> duration = end - start;
    std::cout << "Timeout test duration: " << duration.count() << " s" << std::endl;

    if (result.success) {
        std::cout << "Timeout FAILED: Process succeeded but should have timed out." << std::endl;
        return 1;
    } else {
        std::cout << "Timeout PASSED: Process timed out as expected." << std::endl;
    }

    return 0;
}
