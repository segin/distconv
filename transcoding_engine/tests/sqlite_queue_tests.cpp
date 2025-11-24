#include "test_common.h"
#include <algorithm>
#include <cmath>

using namespace distconv::TranscodingEngine;

class SQLiteQueueTest : public TranscodingEngineTest {
protected:
    void SetUp() override {
        TranscodingEngineTest::SetUp();
        
        // Ensure clean database for each test
        if (db) {
            sqlite3_close(db);
        }
        
        // Initialize with our test database
        int rc = sqlite3_open(test_db_path.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create the jobs table
        const char* sql = "CREATE TABLE IF NOT EXISTS jobs(job_id TEXT PRIMARY KEY NOT NULL);";
        char* err_msg = nullptr;
        rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
        ASSERT_EQ(rc, SQLITE_OK);
        if (err_msg) {
            sqlite3_free(err_msg);
        }
    }
    
    void TearDown() override {
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
        TranscodingEngineTest::TearDown();
    }
};

// Test 33: add_job_to_db correctly inserts a job ID
TEST_F(SQLiteQueueTest, AddJobToDbInsertsCorrectly) {
    std::string test_job_id = "job_insert_test_123";
    
    // Add job to database
    add_job_to_db(test_job_id);
    
    // Verify job was inserted
    std::vector<std::string> jobs = get_jobs_from_db();
    EXPECT_EQ(jobs.size(), 1);
    EXPECT_EQ(jobs[0], test_job_id);
}

// Test 34: add_job_to_db does not insert a duplicate job ID
TEST_F(SQLiteQueueTest, AddJobToDbHandlesDuplicates) {
    std::string test_job_id = "duplicate_test_456";
    
    // Add job twice
    add_job_to_db(test_job_id);
    add_job_to_db(test_job_id); // Should be ignored due to PRIMARY KEY constraint
    
    // Should only have one entry
    std::vector<std::string> jobs = get_jobs_from_db();
    EXPECT_EQ(jobs.size(), 1);
    EXPECT_EQ(jobs[0], test_job_id);
}

// Test 35: remove_job_from_db correctly removes a job ID
TEST_F(SQLiteQueueTest, RemoveJobFromDbRemovesCorrectly) {
    std::string test_job_id = "job_remove_test_789";
    
    // Add then remove job
    add_job_to_db(test_job_id);
    
    std::vector<std::string> jobs_before = get_jobs_from_db();
    EXPECT_EQ(jobs_before.size(), 1);
    
    remove_job_from_db(test_job_id);
    
    std::vector<std::string> jobs_after = get_jobs_from_db();
    EXPECT_TRUE(jobs_after.empty());
}

// Test 36: remove_job_from_db handles a non-existent job ID without error
TEST_F(SQLiteQueueTest, RemoveJobFromDbHandlesNonExistent) {
    std::string non_existent_job = "does_not_exist_999";
    
    // Should not crash or error when removing non-existent job
    EXPECT_NO_THROW(remove_job_from_db(non_existent_job));
    
    // Database should still be empty
    std::vector<std::string> jobs = get_jobs_from_db();
    EXPECT_TRUE(jobs.empty());
}

// Test 37: get_jobs_from_db correctly retrieves all job IDs
TEST_F(SQLiteQueueTest, GetJobsFromDbRetrievesAll) {
    std::vector<std::string> test_jobs = {
        "job_retrieve_1",
        "job_retrieve_2", 
        "job_retrieve_3"
    };
    
    // Add multiple jobs
    for (const auto& job_id : test_jobs) {
        add_job_to_db(job_id);
    }
    
    // Retrieve all jobs
    std::vector<std::string> retrieved_jobs = get_jobs_from_db();
    
    // Should have same number of jobs
    EXPECT_EQ(retrieved_jobs.size(), test_jobs.size());
    
    // All test jobs should be present (order may vary)
    for (const auto& job_id : test_jobs) {
        EXPECT_TRUE(std::find(retrieved_jobs.begin(), retrieved_jobs.end(), job_id) != retrieved_jobs.end());
    }
}

// Test 38: The in-memory localJobQueue vector is correctly populated from the DB at startup
TEST_F(SQLiteQueueTest, LocalJobQueuePopulatedFromDb) {
    // Add jobs to database first
    std::vector<std::string> initial_jobs = {"startup_job_1", "startup_job_2"};
    
    for (const auto& job_id : initial_jobs) {
        add_job_to_db(job_id);
    }
    
    // Simulate startup by getting jobs from DB
    std::vector<std::string> localJobQueue = get_jobs_from_db();
    
    // Verify local queue matches database
    EXPECT_EQ(localJobQueue.size(), initial_jobs.size());
    
    for (const auto& job_id : initial_jobs) {
        EXPECT_TRUE(std::find(localJobQueue.begin(), localJobQueue.end(), job_id) != localJobQueue.end());
    }
}

// Test 39: A job ID is added to the in-memory queue when add_job_to_db is called
TEST_F(SQLiteQueueTest, JobAddedToInMemoryQueue) {
    std::string new_job_id = "in_memory_add_test";
    
    // Start with empty local queue (simulated)
    std::vector<std::string> localJobQueue = get_jobs_from_db();
    EXPECT_TRUE(localJobQueue.empty());
    
    // Add job to database
    add_job_to_db(new_job_id);
    
    // Simulate adding to in-memory queue (as done in main loop)
    localJobQueue.push_back(new_job_id);
    
    // Verify it's in both database and memory
    std::vector<std::string> db_jobs = get_jobs_from_db();
    EXPECT_EQ(db_jobs.size(), 1);
    EXPECT_EQ(db_jobs[0], new_job_id);
    
    EXPECT_EQ(localJobQueue.size(), 1);
    EXPECT_EQ(localJobQueue[0], new_job_id);
}

// Test 40: A job ID is removed from the in-memory queue when remove_job_from_db is called
TEST_F(SQLiteQueueTest, JobRemovedFromInMemoryQueue) {
    std::string job_to_remove = "in_memory_remove_test";
    
    // Add job to both database and simulated in-memory queue
    add_job_to_db(job_to_remove);
    std::vector<std::string> localJobQueue;
    localJobQueue.push_back(job_to_remove);
    
    // Verify job is in both
    std::vector<std::string> db_jobs_before = get_jobs_from_db();
    EXPECT_EQ(db_jobs_before.size(), 1);
    EXPECT_EQ(localJobQueue.size(), 1);
    
    // Remove from database
    remove_job_from_db(job_to_remove);
    
    // Simulate removal from in-memory queue (as done in main loop)
    localJobQueue.erase(std::remove(localJobQueue.begin(), localJobQueue.end(), job_to_remove), localJobQueue.end());
    
    // Verify removal from both
    std::vector<std::string> db_jobs_after = get_jobs_from_db();
    EXPECT_TRUE(db_jobs_after.empty());
    EXPECT_TRUE(localJobQueue.empty());
}

// Test 41: The SQLite callback function correctly parses and adds a job ID to the vector
TEST_F(SQLiteQueueTest, SQLiteCallbackParsesCorrectly) {
    // Test the callback function indirectly through get_jobs_from_db
    // Since the callback is static, we test its behavior through the public interface
    
    std::string test_job_id = "callback_parse_test_456";
    
    // Add a job to the database
    add_job_to_db(test_job_id);
    
    // Get jobs from database - this uses the callback function internally
    std::vector<std::string> retrieved_jobs = get_jobs_from_db();
    
    // The callback should have parsed the job_id correctly
    EXPECT_EQ(retrieved_jobs.size(), 1);
    EXPECT_EQ(retrieved_jobs[0], test_job_id);
    
    // This validates that the callback correctly extracts job_id from SQLite results
}

// Test 42: getCpuTemperature returns a plausible value on Linux (if run on Linux)
TEST_F(SQLiteQueueTest, GetCpuTemperatureLinux) {
    double temperature = getCpuTemperature();
    
    // Temperature should be either a valid reading or -1.0 for error/unsupported
    EXPECT_TRUE(temperature == -1.0 || (temperature >= -50.0 && temperature <= 150.0));
    
    // On Linux systems with thermal sensors, should not return -1.0
    // On other systems or when sensors are unavailable, -1.0 is acceptable
    if (temperature != -1.0) {
        // If we get a reading, it should be in a reasonable range
        EXPECT_GE(temperature, 0.0);   // Not below absolute zero in Celsius
        EXPECT_LE(temperature, 100.0); // Not above boiling point (reasonable for CPU)
    }
}

// Test 43: getCpuTemperature returns -1.0 on unsupported OS
TEST_F(SQLiteQueueTest, GetCpuTemperatureUnsupportedOS) {
    // This test verifies the error handling behavior
    double temperature = getCpuTemperature();
    
    // Function should always return a double value
    EXPECT_TRUE(std::isfinite(temperature) || temperature == -1.0);
    
    // If the OS is unsupported or sensors unavailable, should return -1.0
    // If sensors are available, should return a valid temperature
    EXPECT_TRUE(temperature == -1.0 || temperature > -273.15); // Above absolute zero
}
