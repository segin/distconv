#include "gtest/gtest.h"
#include "../repositories.h"
#include <filesystem>
#include <fstream>

using namespace distconv::DispatchServer;

class SqliteRepositoryTest : public ::testing::Test {
protected:
    std::string db_path = "test_repo.db";

    void SetUp() override {
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
    }

    void TearDown() override {
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
    }
};

TEST_F(SqliteRepositoryTest, BasicCrudOperations) {
    SqliteJobRepository repo(db_path);

    // Create
    std::string job_id = "job_1";
    nlohmann::json job_data = {
        {"job_id", job_id},
        {"status", "pending"},
        {"priority", 10}
    };
    repo.save_job(job_id, job_data);

    // Read
    ASSERT_TRUE(repo.job_exists(job_id));
    nlohmann::json loaded_job = repo.get_job(job_id);
    ASSERT_EQ(loaded_job["status"], "pending");
    ASSERT_EQ(loaded_job["priority"], 10);

    // Update
    job_data["status"] = "processing";
    repo.save_job(job_id, job_data);
    loaded_job = repo.get_job(job_id);
    ASSERT_EQ(loaded_job["status"], "processing");

    // Delete
    repo.remove_job(job_id);
    ASSERT_FALSE(repo.job_exists(job_id));
}

TEST_F(SqliteRepositoryTest, GetAllJobs) {
    SqliteJobRepository repo(db_path);

    repo.save_job("j1", {{"job_id", "j1"}});
    repo.save_job("j2", {{"job_id", "j2"}});

    auto jobs = repo.get_all_jobs();
    ASSERT_EQ(jobs.size(), 2);
}

TEST_F(SqliteRepositoryTest, SpecialCharacters) {
    SqliteJobRepository repo(db_path);
    std::string job_id = "job_'specials'";
    nlohmann::json job_data = {
        {"job_id", job_id},
        {"desc", "contains ' quotes and ; semi-colons -- comments"}
    };
    repo.save_job(job_id, job_data);

    ASSERT_TRUE(repo.job_exists(job_id));
    auto loaded = repo.get_job(job_id);
    ASSERT_EQ(loaded["desc"], "contains ' quotes and ; semi-colons -- comments");
}
