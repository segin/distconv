#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include "../transcoding_engine_core.h"

int main() {
    // Setup test data
    std::vector<std::string> queue;
    for (int i = 0; i < 100; ++i) {
        queue.push_back("job-" + std::to_string(i));
    }

    // Benchmark generateQueueJson
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        std::string json = distconv::TranscodingEngine::generateQueueJson(queue);
        // Prevent optimization
        if (json.empty()) std::cerr << "Error" << std::endl;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "Time taken to generate JSON 1000 times: " << duration << " microseconds" << std::endl;
    std::cout << "Average time per generation: " << duration / 1000.0 << " microseconds" << std::endl;

    // Baseline for cached access (copy string)
    std::string cached_json = distconv::TranscodingEngine::generateQueueJson(queue);
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        std::string json = cached_json; // Simulate access
        // Prevent optimization
        if (json.empty()) std::cerr << "Error" << std::endl;
    }
    end = std::chrono::high_resolution_clock::now();
    auto duration_cached = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "Time taken to access cached JSON 1000 times: " << duration_cached << " microseconds" << std::endl;
    std::cout << "Average time per access: " << duration_cached / 1000.0 << " microseconds" << std::endl;

    if (duration_cached > 0)
        std::cout << "Improvement factor: " << (double)duration / duration_cached << "x" << std::endl;

    return 0;
}
