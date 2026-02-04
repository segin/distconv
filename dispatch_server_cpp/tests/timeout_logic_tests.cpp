#include <gtest/gtest.h>
#include "repositories.h"
#include <filesystem>
#include <chrono>

using namespace distconv::DispatchServer;

class TimeoutLogicTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup SQLite DB
        db_path = "test_timeout.db";
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
    }

    void TearDown() override {
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
    }

    std::string db_path;
};

TEST_F(TimeoutLogicTest, InMemoryRepoFindsTimedOutJobs) {
    InMemoryJobRepository repo;

    auto now = std::chrono::system_clock::now();
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // Job 1: Assigned, Old (Should be timed out)
    nlohmann::json job1;
    job1["job_id"] = "job1";
    job1["status"] = "assigned";
    job1["updated_at"] = now_ms - (60 * 60 * 1000); // 1 hour ago
    repo.save_job("job1", job1);

    // Job 2: Processing, Old (Should be timed out)
    nlohmann::json job2;
    job2["job_id"] = "job2";
    job2["status"] = "processing";
    job2["updated_at"] = now_ms - (60 * 60 * 1000); // 1 hour ago
    repo.save_job("job2", job2);

    // Job 3: Assigned, New (Should NOT be timed out)
    nlohmann::json job3;
    job3["job_id"] = "job3";
    job3["status"] = "assigned";
    job3["updated_at"] = now_ms - (1 * 60 * 1000); // 1 minute ago
    repo.save_job("job3", job3);

    // Job 4: Completed, Old (Should NOT be timed out)
    nlohmann::json job4;
    job4["job_id"] = "job4";
    job4["status"] = "completed";
    job4["updated_at"] = now_ms - (60 * 60 * 1000); // 1 hour ago
    repo.save_job("job4", job4);

    int64_t timeout_ms = 30 * 60 * 1000;
    auto timed_out = repo.get_timed_out_jobs(now_ms - timeout_ms);

    EXPECT_EQ(timed_out.size(), 2);

    bool found1 = false;
    bool found2 = false;
    for (const auto& job : timed_out) {
        if (job["job_id"] == "job1") found1 = true;
        if (job["job_id"] == "job2") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST_F(TimeoutLogicTest, SqliteRepoFindsTimedOutJobs) {
    SqliteJobRepository repo(db_path);

    auto now = std::chrono::system_clock::now();
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // Job 1: Assigned, Old (Should be timed out)
    nlohmann::json job1;
    job1["job_id"] = "job1";
    job1["status"] = "assigned";
    job1["updated_at"] = now_ms - (60 * 60 * 1000); // 1 hour ago
    repo.save_job("job1", job1);

    // Job 2: Processing, Old (Should be timed out)
    nlohmann::json job2;
    job2["job_id"] = "job2";
    job2["status"] = "processing";
    job2["updated_at"] = now_ms - (60 * 60 * 1000); // 1 hour ago
    repo.save_job("job2", job2);

    // Job 3: Assigned, New (Should NOT be timed out)
    nlohmann::json job3;
    job3["job_id"] = "job3";
    job3["status"] = "assigned";
    job3["updated_at"] = now_ms - (1 * 60 * 1000); // 1 minute ago
    repo.save_job("job3", job3);

    // Job 4: Completed, Old (Should NOT be timed out)
    nlohmann::json job4;
    job4["job_id"] = "job4";
    job4["status"] = "completed";
    job4["updated_at"] = now_ms - (60 * 60 * 1000); // 1 hour ago
    repo.save_job("job4", job4);

    int64_t timeout_ms = 30 * 60 * 1000;
    auto timed_out = repo.get_timed_out_jobs(now_ms - timeout_ms);

    EXPECT_EQ(timed_out.size(), 2);

    bool found1 = false;
    bool found2 = false;
    for (const auto& job : timed_out) {
        if (job["job_id"] == "job1") found1 = true;
        if (job["job_id"] == "job2") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}
