#include <gtest/gtest.h>
#include "../repositories.h"
#include <chrono>
#include <filesystem>

using namespace distconv::DispatchServer;

class RepositoryBulkUpdateTest : public ::testing::Test {
protected:
    std::string db_path = "test_bulk_update.db";
    std::unique_ptr<SqliteJobRepository> sqlite_repo;
    std::unique_ptr<InMemoryJobRepository> in_memory_repo;

    void SetUp() override {
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
        sqlite_repo = std::make_unique<SqliteJobRepository>(db_path);
        in_memory_repo = std::make_unique<InMemoryJobRepository>();
    }

    void TearDown() override {
        sqlite_repo.reset();
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
    }
};

TEST_F(RepositoryBulkUpdateTest, SqliteRequeueReadyJobs) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Seed jobs
    nlohmann::json job1;
    job1["job_id"] = "job1";
    job1["status"] = "failed_retry";
    job1["retry_after"] = now - 1000;
    sqlite_repo->save_job("job1", job1);

    nlohmann::json job2;
    job2["job_id"] = "job2";
    job2["status"] = "failed_retry";
    job2["retry_after"] = now + 1000; // Not ready
    sqlite_repo->save_job("job2", job2);

    int requeued = sqlite_repo->requeue_ready_jobs(now);
    EXPECT_EQ(requeued, 1);

    auto updated_job1 = sqlite_repo->get_job("job1");
    EXPECT_EQ(updated_job1["status"], "pending");

    auto updated_job2 = sqlite_repo->get_job("job2");
    EXPECT_EQ(updated_job2["status"], "failed_retry");
}

TEST_F(RepositoryBulkUpdateTest, InMemoryRequeueReadyJobs) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Seed jobs
    nlohmann::json job1;
    job1["job_id"] = "job1";
    job1["status"] = "failed_retry";
    job1["retry_after"] = now - 1000;
    in_memory_repo->save_job("job1", job1);

    nlohmann::json job2;
    job2["job_id"] = "job2";
    job2["status"] = "failed_retry";
    job2["retry_after"] = now + 1000; // Not ready
    in_memory_repo->save_job("job2", job2);

    int requeued = in_memory_repo->requeue_ready_jobs(now);
    EXPECT_EQ(requeued, 1);

    auto updated_job1 = in_memory_repo->get_job("job1");
    EXPECT_EQ(updated_job1["status"], "pending");

    auto updated_job2 = in_memory_repo->get_job("job2");
    EXPECT_EQ(updated_job2["status"], "failed_retry");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
