#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <functional>
#include <stdexcept>

// Compile with:
// g++ -std=c++17 -o benchmark_polling transcoding_engine/tests/benchmark_polling_legacy.cpp transcoding_engine/transcoding_engine_core.cpp transcoding_engine/tests/mocks/mock_libs.cpp -I transcoding_engine/tests/mocks/include -lsqlite3 -pthread

// Include the header under test
#include "../transcoding_engine_core.h"

// Define a custom exception to break the infinite loop
class BenchmarkCompleteException : public std::exception {};

// Mock Data
const int JOBS_TO_PROCESS = 5;
int jobs_processed = 0;

std::string mock_getJob(const std::string&, const std::string&, const std::string&, const std::string&) {
    if (jobs_processed >= JOBS_TO_PROCESS) {
        throw BenchmarkCompleteException();
    }
    // Return a valid JSON job
    return "{\"job_id\": \"job-" + std::to_string(jobs_processed) + "\", \"source_url\": \"http://src\", \"target_codec\": \"h264\"}";
}

void mock_performTranscoding(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&, const std::string&) {
    // Simulate some work (very fast for benchmark)
    // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    jobs_processed++;
    std::cout << "Mock Transcoded Job " << jobs_processed << std::endl;
}

int main() {
    std::cout << "Running Benchmark with " << JOBS_TO_PROCESS << " jobs..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Pass dummy args
        char* argv[] = {(char*)"benchmark", NULL};
        distconv::TranscodingEngine::run_transcoding_engine(1, argv, mock_getJob, mock_performTranscoding);
    } catch (const BenchmarkCompleteException&) {
        std::cout << "Benchmark loop finished." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected exception: " << e.what() << std::endl;
        return 1;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Total time: " << duration << " ms" << std::endl;
    std::cout << "Average time per job: " << (double)duration / JOBS_TO_PROCESS << " ms" << std::endl;

    if (duration > JOBS_TO_PROCESS * 1000) {
        std::cout << "Result: SLOW (Expected due to 1s sleep)" << std::endl;
    } else {
        std::cout << "Result: FAST" << std::endl;
    }

    return 0;
}
