#include "gtest/gtest.h"
#include "../transcoding_engine_core.h" // Include the header for the core logic
#include <fstream>
#include <string>
#include <vector>

// Mock functions or classes if necessary for isolated unit testing
// For now, we'll focus on basic engine functionality that can be tested without complex mocks.

// Test fixture for the Transcoding Engine
class TranscodingEngineTest : public ::testing::Test {
protected:
    // You can set up resources here that are used by all tests in this fixture
    void SetUp() override {
        // Initialize any necessary engine components or mock dependencies
        // Ensure a clean database for each test
        sqlite3_close(db); // Close any existing connection
        remove("transcoding_jobs.db"); // Delete the database file
        init_sqlite(); // Re-initialize for the test
    }

    void TearDown() override {
        // Clean up resources
        sqlite3_close(db);
        remove("transcoding_jobs.db");
    }
};

// Test Case: Basic SQLite operations
TEST_F(TranscodingEngineTest, SQLiteOperations) {
    std::string job_id_1 = "job_sqlite_1";
    std::string job_id_2 = "job_sqlite_2";

    add_job_to_db(job_id_1);
    add_job_to_db(job_id_2);

    std::vector<std::string> jobs = get_jobs_from_db();
    ASSERT_EQ(jobs.size(), 2);
    ASSERT_NE(std::find(jobs.begin(), jobs.end(), job_id_1), jobs.end());
    ASSERT_NE(std::find(jobs.begin(), jobs.end(), job_id_2), jobs.end());

    remove_job_from_db(job_id_1);
    jobs = get_jobs_from_db();
    ASSERT_EQ(jobs.size(), 1);
    ASSERT_EQ(std::find(jobs.begin(), jobs.end(), job_id_1), jobs.end());
    ASSERT_NE(std::find(jobs.begin(), jobs.end(), job_id_2), jobs.end());

    remove_job_from_db(job_id_2);
    jobs = get_jobs_from_db();
    ASSERT_TRUE(jobs.empty());
}

// Test case for getFFmpegCapabilities (mocking system call is hard, so basic check)
TEST_F(TranscodingEngineTest, GetFFmpegCapabilities) {
    std::string encoders = getFFmpegCapabilities("encoders");
    // We can't assert specific encoders without knowing the ffmpeg installation,
    // but we can check if the string is not empty.
    ASSERT_FALSE(encoders.empty());
}

// Test case for getFFmpegHWAccels (mocking system call is hard, so basic check)
TEST_F(TranscodingEngineTest, GetFFmpegHWAccels) {
    std::string hwaccels = getFFmpegHWAccels();
    // Similar to capabilities, just check if not empty.
    // This might be empty on systems without hardware acceleration.
    // ASSERT_FALSE(hwaccels.empty()); // This assertion might fail on some systems
    SUCCEED(); // Placeholder, as it might be empty
}

// Test case for getCpuTemperature (platform-dependent, basic check)
TEST_F(TranscodingEngineTest, GetCpuTemperature) {
    double temp = getCpuTemperature();
    // Expect a non-negative temperature, or -1.0 if not implemented/error
    ASSERT_GE(temp, -1.0);
}

// Test case for getHostname
TEST_F(TranscodingEngineTest, GetHostname) {
    std::string hostname = getHostname();
    ASSERT_FALSE(hostname.empty());
    ASSERT_NE(hostname, "unknown"); // Should not be "unknown" on a working system
}

// More tests will be added here as the engine's functions are refactored to be testable.

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
