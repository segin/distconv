#include <gtest/gtest.h>
#include "implementations/secure_subprocess.h"
#include <chrono>
#include <iostream>

using namespace distconv::TranscodingEngine;

class PerformanceReproductionTest : public ::testing::Test {
};

// Test to demonstrate blocking I/O bypassing timeout
TEST_F(PerformanceReproductionTest, TimeoutRespectsBlockingIO) {
    SecureSubprocess subprocess;

    // Command that sleeps for 3 seconds, but we set timeout to 1 second
    // The current implementation might block on waiting for pipes to close (which happens after sleep)
    // thus ignoring the 1s timeout.
    // "sleep 3" itself doesn't write to pipes, so the 'read' might block waiting for data/EOF.
    // Since 'sleep' keeps the process alive, the pipe write-end remains open.
    // The parent 'read' blocks until 'sleep' exits (closing the pipe).
    std::vector<std::string> command = {"sh", "-c", "sleep 3; echo done"};

    auto start = std::chrono::steady_clock::now();

    // Set timeout to 1 second
    auto result = subprocess.run(command, "", 1);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Execution took: " << duration.count() << "ms" << std::endl;

    // We expect the timeout to kick in around 1000ms.
    // If it takes ~3000ms, the timeout was ignored.
    // We allow some margin for process overhead (e.g., < 1500ms).

    // NOTE: This assertion is expected to FAIL before the fix.
    // We assert that it *failed* (timed out) properly.
    // The result.success should be false due to timeout.
    // And result.stdout_output should be empty or partial?
    // Actually, if it times out, we kill it.

    EXPECT_LT(duration.count(), 1500) << "Timeout mechanism failed! Operation took too long.";
}
