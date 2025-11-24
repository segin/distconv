#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <future>
#include <set>
#include "dispatch_server_core.h"
#include "test_common.h"

using namespace distconv::DispatchServer;

class ThreadSafetyImprovementsTest : public ApiTest {
protected:
    void SetUp() override {
        ApiTest::SetUp();
        
        // Enable mocking to avoid file I/O during tests
        mock_save_state_enabled = true;
        save_state_call_count = 0;
    }

    void TearDown() override {
        // Disable mocking after each test
        mock_save_state_enabled = false;
        ApiTest::TearDown();
    }
};

// Test 1: UUID Generation for Job IDs
TEST_F(ThreadSafetyImprovementsTest, UUIDJobIdGeneration) {
    // Submit multiple jobs concurrently
    std::vector<std::future<std::string>> futures;
    const int num_jobs = 10;
    
    for (int i = 0; i < num_jobs; ++i) {
        futures.push_back(std::async(std::launch::async, [this]() {
            nlohmann::json job_data;
            job_data["source_url"] = "http://example.com/video.mp4";
            job_data["target_codec"] = "libx264";
            job_data["job_size"] = 100.0;
            job_data["priority"] = 1;
            
            auto response = client->Post("/jobs/", admin_headers, job_data.dump(), "application/json");
            return response->body;
        }));
    }
    
    // Collect all job IDs
    std::vector<std::string> job_ids;
    for (auto& future : futures) {
        std::string response = future.get();
        
        // Extract job ID from response
        nlohmann::json response_json = nlohmann::json::parse(response);
        ASSERT_TRUE(response_json.contains("job_id"));
        job_ids.push_back(response_json["job_id"]);
    }
    
    // Verify all job IDs are unique and are valid UUIDs
    std::set<std::string> unique_ids(job_ids.begin(), job_ids.end());
    EXPECT_EQ(unique_ids.size(), num_jobs);
    
    // Verify UUID format (36 characters with hyphens)
    for (const auto& id : job_ids) {
        EXPECT_EQ(id.length(), 36);
        EXPECT_EQ(id[8], '-');
        EXPECT_EQ(id[13], '-');
        EXPECT_EQ(id[18], '-');
        EXPECT_EQ(id[23], '-');
    }
}

// Test 2: Job Priority Support
TEST_F(ThreadSafetyImprovementsTest, JobPrioritySupport) {
    // Submit jobs with different priorities
    std::vector<std::pair<std::string, int>> job_priorities = {
        {"urgent_job", 2},
        {"high_job", 1},
        {"normal_job", 0}
    };
    
    for (const auto& [job_name, priority] : job_priorities) {
        nlohmann::json job_data;
        job_data["source_url"] = "http://example.com/" + job_name + ".mp4";
        job_data["target_codec"] = "libx264";
        job_data["priority"] = priority;
        
        auto response = client->Post("/jobs/", admin_headers, job_data.dump(), "application/json");
        EXPECT_EQ(response->status, 200);
        
        nlohmann::json response_json = nlohmann::json::parse(response->body);
        ASSERT_TRUE(response_json.contains("job_id"));
        
        // Verify job was stored with correct priority
        std::string job_id = response_json["job_id"];
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            ASSERT_TRUE(jobs_db.contains(job_id));
            EXPECT_EQ(jobs_db[job_id]["priority"], priority);
        }
    }
}

// Test 3: Enhanced Job Data Structure
TEST_F(ThreadSafetyImprovementsTest, EnhancedJobDataStructure) {
    nlohmann::json job_data;
    job_data["source_url"] = "http://example.com/video.mp4";
    job_data["target_codec"] = "libx264";
    job_data["job_size"] = 150.5;
    job_data["max_retries"] = 5;
    job_data["priority"] = 1;
    
    auto response = client->Post("/jobs/", admin_headers, job_data.dump(), "application/json");
    EXPECT_EQ(response->status, 200);
    
    nlohmann::json response_json = nlohmann::json::parse(response->body);
    std::string job_id = response_json["job_id"];
    
    // Verify job structure includes all new fields
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.contains(job_id));
        const auto& job = jobs_db[job_id];
        
        EXPECT_TRUE(job.contains("created_at"));
        EXPECT_TRUE(job.contains("updated_at"));
        EXPECT_EQ(job["priority"], 1);
        EXPECT_EQ(job["max_retries"], 5);
        EXPECT_EQ(job["job_size"], 150.5);
        EXPECT_EQ(job["retries"], 0);
        
        // Verify timestamps are reasonable
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto created_at = job["created_at"].get<int64_t>();
        auto updated_at = job["updated_at"].get<int64_t>();
        
        EXPECT_GT(created_at, now - 5000); // Within last 5 seconds
        EXPECT_LT(created_at, now + 1000);  // Not in the future
        EXPECT_EQ(created_at, updated_at);  // Should be same for new job
    }
}

// Test 4: UUID Generation Testing
TEST_F(ThreadSafetyImprovementsTest, UUIDGenerationTest) {
    // Test UUID generation function directly
    std::string uuid1 = generate_uuid();
    std::string uuid2 = generate_uuid();
    
    // Verify UUIDs are different
    EXPECT_NE(uuid1, uuid2);
    
    // Verify UUID format (36 characters with hyphens)
    EXPECT_EQ(uuid1.length(), 36);
    EXPECT_EQ(uuid1[8], '-');
    EXPECT_EQ(uuid1[13], '-');
    EXPECT_EQ(uuid1[18], '-');
    EXPECT_EQ(uuid1[23], '-');
    
    EXPECT_EQ(uuid2.length(), 36);
    EXPECT_EQ(uuid2[8], '-');
    EXPECT_EQ(uuid2[13], '-');
    EXPECT_EQ(uuid2[18], '-');
    EXPECT_EQ(uuid2[23], '-');
}

// Test 5: Job and Engine Domain Classes
TEST_F(ThreadSafetyImprovementsTest, JobAndEngineStructSerialization) {
    // Test Job struct
    Job job;
    job.id = "test-job-123";
    job.status = "pending";
    job.source_url = "http://example.com/video.mp4";
    job.codec = "libx264";
    job.job_size = 100.5;
    job.priority = 1;
    job.max_retries = 3;
    job.retries = 0;
    job.created_at = std::chrono::system_clock::now();
    job.updated_at = job.created_at;
    
    // Test serialization
    nlohmann::json job_json = job.to_json();
    EXPECT_EQ(job_json["id"], "test-job-123");
    EXPECT_EQ(job_json["status"], "pending");
    EXPECT_EQ(job_json["priority"], 1);
    EXPECT_EQ(job_json["job_size"], 100.5);
    
    // Test deserialization
    Job deserialized_job = Job::from_json(job_json);
    EXPECT_EQ(deserialized_job.id, job.id);
    EXPECT_EQ(deserialized_job.status, job.status);
    EXPECT_EQ(deserialized_job.priority, job.priority);
    EXPECT_EQ(deserialized_job.job_size, job.job_size);
    
    // Test Engine struct
    Engine engine;
    engine.id = "test-engine-456";
    engine.hostname = "test-host";
    engine.status = "idle";
    engine.benchmark_time = 50.0;
    engine.can_stream = true;
    engine.storage_capacity_gb = 500;
    engine.last_heartbeat = std::chrono::system_clock::now();
    
    // Test serialization
    nlohmann::json engine_json = engine.to_json();
    EXPECT_EQ(engine_json["id"], "test-engine-456");
    EXPECT_EQ(engine_json["hostname"], "test-host");
    EXPECT_EQ(engine_json["can_stream"], true);
    EXPECT_EQ(engine_json["storage_capacity_gb"], 500);
    
    // Test deserialization
    Engine deserialized_engine = Engine::from_json(engine_json);
    EXPECT_EQ(deserialized_engine.id, engine.id);
    EXPECT_EQ(deserialized_engine.hostname, engine.hostname);
    EXPECT_EQ(deserialized_engine.can_stream, engine.can_stream);
    EXPECT_EQ(deserialized_engine.storage_capacity_gb, engine.storage_capacity_gb);
}
