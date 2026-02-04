#include "gtest/gtest.h"
#include "../repositories.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <vector>
#include <thread>
#include <chrono>

using namespace distconv::DispatchServer;

class SqliteJobRepositoryTest : public ::testing::Test {
protected:
    std::string db_path = "test_jobs.db";
    SqliteJobRepository* repo;

    void SetUp() override {
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
        repo = new SqliteJobRepository(db_path);
    }

    void TearDown() override {
        delete repo;
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
    }
};

TEST_F(SqliteJobRepositoryTest, SaveAndGetJob) {
    nlohmann::json job;
    job["job_id"] = "job1";
    job["status"] = "pending";
    job["priority"] = 1;

    repo->save_job("job1", job);

    nlohmann::json retrieved = repo->get_job("job1");
    ASSERT_EQ(retrieved["job_id"], "job1");
    ASSERT_EQ(retrieved["status"], "pending");
    ASSERT_EQ(retrieved["priority"], 1);
}

TEST_F(SqliteJobRepositoryTest, GetNextPendingJobPriority) {
    std::vector<std::string> engines;

    // Job 1: Priority 0
    nlohmann::json job1;
    job1["job_id"] = "job1";
    job1["status"] = "pending";
    job1["priority"] = 0;
    repo->save_job("job1", job1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Ensure timestamp diff

    // Job 2: Priority 2 (High)
    nlohmann::json job2;
    job2["job_id"] = "job2";
    job2["status"] = "pending";
    job2["priority"] = 2;
    repo->save_job("job2", job2);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Job 3: Priority 2 (High) - Newer than job2
    nlohmann::json job3;
    job3["job_id"] = "job3";
    job3["status"] = "pending";
    job3["priority"] = 2;
    repo->save_job("job3", job3);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Job 4: Priority 1
    nlohmann::json job4;
    job4["job_id"] = "job4";
    job4["status"] = "pending";
    job4["priority"] = 1;
    repo->save_job("job4", job4);

    // Expected order:
    // 1. Priority 2, oldest -> job2
    // 2. Priority 2, newer -> job3
    // 3. Priority 1 -> job4
    // 4. Priority 0 -> job1

    // 1st
    nlohmann::json next = repo->get_next_pending_job(engines);
    ASSERT_EQ(next["job_id"], "job2");

    // Mark job2 as assigned so we get the next one
    job2["status"] = "assigned";
    repo->save_job("job2", job2);

    // 2nd
    next = repo->get_next_pending_job(engines);
    ASSERT_EQ(next["job_id"], "job3");

    // Mark job3 as assigned
    job3["status"] = "assigned";
    repo->save_job("job3", job3);

    // 3rd
    next = repo->get_next_pending_job(engines);
    ASSERT_EQ(next["job_id"], "job4");

    // Mark job4 as assigned
    job4["status"] = "assigned";
    repo->save_job("job4", job4);

    // 4th
    next = repo->get_next_pending_job(engines);
    ASSERT_EQ(next["job_id"], "job1");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
