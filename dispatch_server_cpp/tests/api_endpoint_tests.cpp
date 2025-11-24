#include <gtest/gtest.h>
#include "repositories.h"
#include "job_update_handler.h"
#include "storage_pool_handler.h"
#include "httplib.h"
#include <regex>

using namespace distconv::DispatchServer;

// Test job update functionality
TEST(APIEndpointsTest, UpdateJobPriority) {
    auto job_repo = std::make_shared<InMemoryJobRepository>();
    
    // Create a test job
    nlohmann::json job;
    job["job_id"] = "test-job-1";
    job["status"] = "pending";
    job["priority"] = 5;
    job["max_retries"] = 3;
    job_repo->save_job("test-job-1", job);
    
    // Update priority
    nlohmann::json updates;
    updates["priority"] = 10;
    
    ASSERT_TRUE(job_repo->update_job("test-job-1", updates));
    
    nlohmann::json updated_job = job_repo->get_job("test-job-1");
    EXPECT_EQ(updated_job["priority"], 10);
    EXPECT_EQ(updated_job["max_retries"], 3); // Should remain unchanged
}

// Test get jobs by engine
TEST(APIEndpointsTest, GetJobsByEngine) {
    auto job_repo = std::make_shared<InMemoryJobRepository>();
    
    // Create test jobs
    nlohmann::json job1, job2, job3;
    job1["job_id"] = "job-1";
    job1["assigned_engine"] = "engine-1";
    job1["status"] = "assigned";
    
    job2["job_id"] = "job-2";
    job2["assigned_engine"] = "engine-2";
    job2["status"] = "assigned";
    
    job3["job_id"] = "job-3";
    job3["assigned_engine"] = "engine-1";
    job3["status"] = "completed";
    
    job_repo->save_job("job-1", job1);
    job_repo->save_job("job-2", job2);
    job_repo->save_job("job-3", job3);
    
    // Get jobs for engine-1
    std::vector<nlohmann::json> engine1_jobs = job_repo->get_jobs_by_engine("engine-1");
    
    EXPECT_EQ(engine1_jobs.size(), 2);
    EXPECT_TRUE(std::any_of(engine1_jobs.begin(), engine1_jobs.end(),
        [](const nlohmann::json& j) { return j["job_id"] == "job-1"; }));
    EXPECT_TRUE(std::any_of(engine1_jobs.begin(), engine1_jobs.end(),
        [](const nlohmann::json& j) { return j["job_id"] == "job-3"; }));
}

// Test update job progress
TEST(APIEndpointsTest, UpdateJobProgress) {
    auto job_repo = std::make_shared<InMemoryJobRepository>();
    
    // Create a test job
    nlohmann::json job;
    job["job_id"] = "test-job-1";
    job["status"] = "assigned";
    job_repo->save_job("test-job-1", job);
    
    // Update progress
    ASSERT_TRUE(job_repo->update_job_progress("test-job-1", 50, "Processing video"));
    
    nlohmann::json updated_job = job_repo->get_job("test-job-1");
    EXPECT_EQ(updated_job["progress"], 50);
    EXPECT_EQ(updated_job["progress_message"], "Processing video");
}

// Test storage pool CRUD operations
TEST(APIEndpointsTest, StoragePoolCRUD) {
    auto storage_repo = std::make_shared<InMemoryStorageRepository>();
    
    // Create
    nlohmann::json pool;
    pool["id"] = "pool-1";
    pool["name"] = "Main Storage";
    pool["capacity_gb"] = 1000;
    pool["used_gb"] = 500;
    storage_repo->save_pool("pool-1", pool);
    
    // Read
    nlohmann::json retrieved_pool = storage_repo->get_pool("pool-1");
    EXPECT_EQ(retrieved_pool["name"], "Main Storage");
    EXPECT_EQ(retrieved_pool["capacity_gb"], 1000);
    
    // Update
    retrieved_pool["used_gb"] = 600;
    storage_repo->save_pool("pool-1", retrieved_pool);
    nlohmann::json updated_pool = storage_repo->get_pool("pool-1");
    EXPECT_EQ(updated_pool["used_gb"], 600);
    
    // List
    nlohmann::json pool2;
    pool2["id"] = "pool-2";
    pool2["name"] = "Backup Storage";
    storage_repo->save_pool("pool-2", pool2);
    
    std::vector<nlohmann::json> all_pools = storage_repo->get_all_pools();
    EXPECT_EQ(all_pools.size(), 2);
    
    // Delete
    storage_repo->remove_pool("pool-1");
    EXPECT_FALSE(storage_repo->pool_exists("pool-1"));
    EXPECT_TRUE(storage_repo->pool_exists("pool-2"));
}

// Test unified status endpoint logic
TEST(APIEndpointsTest, UnifiedStatusComplete) {
   auto job_repo = std::make_shared<InMemoryJobRepository>();
    
    // Create a test job
    nlohmann::json job;
    job["job_id"] = "test-job-1";
    job["status"] = "assigned";
    job["retries"] = 0;
    job["max_retries"] = 3;
    job_repo->save_job("test-job-1", job);
    
    // Simulate completing the job
    nlohmann::json updates;
    updates["status"] = "completed";
    updates["output_url"] = "http://example.com/output.mp4";
    
    job_repo->update_job("test-job-1", updates);
    
    nlohmann::json updated_job = job_repo->get_job("test-job-1");
    EXPECT_EQ(updated_job["status"], "completed");
}

// Test  retry logic for failed jobs
TEST(APIEndpointsTest, FailedJobRetryLogic) {
    auto job_repo = std::make_shared<InMemoryJobRepository>();
    
    // Create a test job
    nlohmann::json job;
    job["job_id"] = "test-job-1";
    job["status"] = "assigned";
    job["retries"] = 1;
    job["max_retries"] = 3;
    job_repo->save_job("test-job-1", job);
    
    // Mark as failed_retry
    auto now = std::chrono::system_clock::now();
    auto retry_after = now + std::chrono::seconds(10);
    int64_t retry_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        retry_after.time_since_epoch()).count();
    
    job_repo->mark_job_as_failed_retry("test-job-1", retry_timestamp);
    
    nlohmann::json updated_job = job_repo->get_job("test-job-1");
    EXPECT_EQ(updated_job["status"], "failed_retry");
    EXPECT_EQ(updated_job["retry_after"], retry_timestamp);
}
