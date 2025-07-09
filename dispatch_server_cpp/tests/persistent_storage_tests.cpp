#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <set> // For checking uniqueness of job IDs

#include "test_common.h"

TEST_F(ApiTest, SaveStateWritesJobsAndEngines) {
    // 1. Create a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 3. Save the state
    save_state();

    // 4. Read the state file and verify its contents
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("jobs"));
    ASSERT_TRUE(state["jobs"].contains(job_id));
    ASSERT_EQ(state["jobs"][job_id]["source_url"], "http://example.com/video.mp4");

    ASSERT_TRUE(state.contains("engines"));
    ASSERT_TRUE(state["engines"].contains("engine-123"));
    ASSERT_EQ(state["engines"]["engine-123"]["status"], "idle");
}

TEST_F(ApiTest, LoadStateLoadsJobs) {
    // 1. Create a job and save the state to a temporary file
    std::string temp_storage_file = "temp_state.json";
    PERSISTENT_STORAGE_FILE = temp_storage_file;
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    save_state();

    // 2. Clear the in-memory database
    clear_db();

    // 3. Load the state from the temporary file
    load_state();

    // 4. Verify that the job is loaded into the in-memory database
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.contains(job_id));
        ASSERT_EQ(jobs_db[job_id]["source_url"], "http://example.com/video.mp4");
    }

    // 5. Clean up the temporary file
    std::remove(temp_storage_file.c_str());
}

TEST_F(ApiTest, LoadStateLoadsEngines) {
    // 1. Register an engine and save the state to a temporary file
    std::string temp_storage_file = "temp_state.json";
    PERSISTENT_STORAGE_FILE = temp_storage_file;
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    save_state();

    // 2. Clear the in-memory database
    clear_db();

    // 3. Load the state from the temporary file
    load_state();

    // 4. Verify that the engine is loaded into the in-memory database
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(engines_db.contains("engine-123"));
        ASSERT_EQ(engines_db["engine-123"]["status"], "idle");
    }

    // 5. Clean up the temporary file
    std::remove(temp_storage_file.c_str());
}

TEST_F(ApiTest, LoadStateHandlesNonExistentFile) {
    // 1. Ensure the persistent storage file does not exist
    std::remove(PERSISTENT_STORAGE_FILE.c_str());

    // 2. Load the state
    load_state();

    // 3. Verify that the in-memory databases are empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.empty());
    }
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(engines_db.empty());
    }
}

TEST_F(ApiTest, LoadStateHandlesCorruptFile) {
    // 1. Create a corrupt persistent storage file
    std::ofstream ofs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ofs.is_open());
    ofs << "{'invalid_json': }";
    ofs.close();

    // 2. Load the state
    load_state();

    // 3. Verify that the in-memory databases are empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.empty());
    }
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(engines_db.empty());
    }
}

TEST_F(ApiTest, LoadStateHandlesEmptyFile) {
    // 1. Create an empty persistent storage file
    std::ofstream ofs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ofs.is_open());
    ofs << "";
    ofs.close();

    // 2. Load the state
    load_state();

    // 3. Verify that the in-memory databases are empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.empty());
    }
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(engines_db.empty());
    }
}

TEST_F(ApiTest, StateIsPreservedAfterRestart) {
    // 1. Create a job and register an engine
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 2. Save the state
    save_state();

    // 3. Clear the in-memory database
    clear_db();

    // 4. Load the state
    load_state();

    // 5. Verify that the job and engine are loaded correctly
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.contains(job_id));
        ASSERT_EQ(jobs_db[job_id]["source_url"], "http://example.com/video.mp4");
    }
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(engines_db.contains("engine-123"));
        ASSERT_EQ(engines_db["engine-123"]["status"], "idle");
    }
}

TEST_F(ApiTest, SubmitJobTriggersSaveState) {
    // 1. Submit a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Verify that the state file was written to
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("jobs"));
    ASSERT_TRUE(state["jobs"].contains(job_id));
}

TEST_F(ApiTest, HeartbeatTriggersSaveState) {
    // 1. Send a heartbeat
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 2. Verify that the state file was written to
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("engines"));
    ASSERT_TRUE(state["engines"].contains("engine-123"));
}

TEST_F(ApiTest, CompleteJobTriggersSaveState) {
    // 1. Create a job and assign it to an engine
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 2. Mark the job as complete
    nlohmann::json complete_payload = {
        {"output_url", "http://example.com/video_transcoded.mp4"}
    };
    auto res_complete = client->Post("/jobs/" + job_id + "/complete", admin_headers, complete_payload.dump(), "application/json");
    ASSERT_EQ(res_complete->status, 200);

    // 3. Verify that the state file was written to
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("jobs"));
    ASSERT_TRUE(state["jobs"].contains(job_id));
    ASSERT_EQ(state["jobs"][job_id]["status"], "completed");
}

TEST_F(ApiTest, FailJobTriggersSaveState) {
    // 1. Create a job and assign it to an engine
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 2. Mark the job as failed
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post("/jobs/" + job_id + "/fail", admin_headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);

    // 3. Verify that the state file was written to
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("jobs"));
    ASSERT_TRUE(state["jobs"].contains(job_id));
    ASSERT_EQ(state["jobs"][job_id]["status"], "pending"); // Re-queued
}

TEST_F(ApiTest, AssignJobTriggersSaveState) {
    // 1. Create a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);

    // 3. Assign the job to the engine
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", admin_headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);

    // 4. Verify that the state file was written to
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("jobs"));
    ASSERT_TRUE(state["jobs"].contains(job_id));
    ASSERT_EQ(state["jobs"][job_id]["status"], "assigned");
}

TEST_F(ApiTest, SaveStateWritesToTemporaryFileAndRenames) {
    // 1. Create a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Call save_state (which should use a temporary file and rename)
    save_state();

    // 3. Verify that the original file exists and contains the data
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("jobs"));
    ASSERT_TRUE(state["jobs"].contains(job_id));

    // 4. Verify that no temporary file exists
    std::string temp_file = PERSISTENT_STORAGE_FILE + ".tmp";
    ASSERT_FALSE(std::ifstream(temp_file).good());
}

TEST_F(ApiTest, LoadStateHandlesEmptyJobsOrEnginesKeys) {
    // 1. Create a state file with empty "jobs" and "engines" arrays
    std::string temp_storage_file = "temp_state.json";
    PERSISTENT_STORAGE_FILE = temp_storage_file;
    nlohmann::json empty_state = {
        {"jobs", nlohmann::json::array()},
        {"engines", nlohmann::json::array()}
    };
    std::ofstream ofs(temp_storage_file);
    ofs << empty_state.dump(4);
    ofs.close();

    // 2. Load the state
    load_state();

    // 3. Verify that the in-memory databases are empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.empty());
        ASSERT_TRUE(engines_db.empty());
    }

    // 4. Clean up the temporary file
    std::remove(temp_storage_file.c_str());
}

TEST_F(ApiTest, LoadStateHandlesOnlyJobsKey) {
    // 1. Create a state file with only a "jobs" key
    std::string temp_storage_file = "temp_state.json";
    PERSISTENT_STORAGE_FILE = temp_storage_file;
    nlohmann::json state_with_only_jobs = {
        {"jobs", {{"job1", {{"source_url", "url1"}}}}}
    };
    std::ofstream ofs(temp_storage_file);
    ofs << state_with_only_jobs.dump(4);
    ofs.close();

    // 2. Load the state
    load_state();

    // 3. Verify that jobs are loaded and engines are empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.contains("job1"));
        ASSERT_EQ(jobs_db["job1"]["source_url"], "url1");
        ASSERT_TRUE(engines_db.empty());
    }

    // 4. Clean up the temporary file
    std::remove(temp_storage_file.c_str());
}

TEST_F(ApiTest, LoadStateWithSingleJob) {
    // 1. Create a temporary state file with a single job
    std::string temp_storage_file = "temp_state_single_job.json";
    PERSISTENT_STORAGE_FILE = temp_storage_file;
    nlohmann::json single_job_state = {
        {"jobs", {
            {"job_id_1", {
                {"job_id", "job_id_1"},
                {"source_url", "http://example.com/video_single.mp4"},
                {"target_codec", "h264"},
                {"status", "pending"}
            }}
        }},
        {"engines", nlohmann::json::object()} // Ensure engines is an empty object
    };
    std::ofstream ofs(temp_storage_file);
    ofs << single_job_state.dump(4);
    ofs.close();

    // 2. Clear the in-memory database to ensure a clean load
    clear_db();

    // 3. Load the state from the temporary file
    load_state();

    // 4. Verify that the job is loaded correctly and engines_db is empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_EQ(jobs_db.size(), 1);
        ASSERT_TRUE(jobs_db.contains("job_id_1"));
        ASSERT_EQ(jobs_db["job_id_1"]["source_url"], "http://example.com/video_single.mp4");
        ASSERT_EQ(jobs_db["job_id_1"]["status"], "pending");
        ASSERT_TRUE(engines_db.empty());
    }

    // 5. Clean up the temporary file
    std::remove(temp_storage_file.c_str());
}

TEST_F(ApiTest, LoadStateWithSingleEngine) {
    // 1. Create a temporary state file with a single engine
    std::string temp_storage_file = "temp_state_single_engine.json";
    PERSISTENT_STORAGE_FILE = temp_storage_file;
    nlohmann::json single_engine_state = {
        {"jobs", nlohmann::json::object()}, // Ensure jobs is an empty object
        {"engines", {
            {"engine_id_1", {
                {"engine_id", "engine_id_1"},
                {"engine_type", "transcoder"},
                {"supported_codecs", {"h264", "vp9"}},
                {"status", "idle"},
                {"benchmark_time", 100.0}
            }}
        }}
    };
    std::ofstream ofs(temp_storage_file);
    ofs << single_engine_state.dump(4);
    ofs.close();

    // 2. Clear the in-memory database to ensure a clean load
    clear_db();

    // 3. Load the state from the temporary file
    load_state();

    // 4. Verify that the engine is loaded correctly and jobs_db is empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.empty());
        ASSERT_EQ(engines_db.size(), 1);
        ASSERT_TRUE(engines_db.contains("engine_id_1"));
        ASSERT_EQ(engines_db["engine_id_1"]["engine_type"], "transcoder");
        ASSERT_EQ(engines_db["engine_id_1"]["status"], "idle");
    }

    // 5. Clean up the temporary file
    std::remove(temp_storage_file.c_str());
}

TEST_F(ApiTest, SaveStateWithSingleJob) {
    // 1. Create a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video_single_save.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];

    // 2. Save the state
    save_state();

    // 3. Read the state file and verify its contents
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("jobs"));
    ASSERT_TRUE(state["jobs"].contains(job_id));
    ASSERT_EQ(state["jobs"][job_id]["source_url"], "http://example.com/video_single_save.mp4");
    ASSERT_EQ(state["jobs"][job_id]["status"], "pending");
}

TEST_F(ApiTest, SaveStateWithSingleEngine) {
    // 1. Register an engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine_single_save"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", admin_headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    std::string engine_id = "engine_single_save";

    // 2. Save the state
    save_state();

    // 3. Read the state file and verify its contents
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("engines"));
    ASSERT_TRUE(state["engines"].contains(engine_id));
    ASSERT_EQ(state["engines"][engine_id]["engine_type"], "transcoder");
    ASSERT_EQ(state["engines"][engine_id]["status"], "idle");
}

TEST_F(ApiTest, SaveStateWithZeroJobsAndZeroEngines) {
    // 1. Clear the in-memory databases
    clear_db();

    // 2. Save the state
    save_state();

    // 3. Read the state file and verify its contents
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("jobs"));
    ASSERT_TRUE(state["jobs"].is_object());
    ASSERT_TRUE(state["jobs"].empty());

    ASSERT_TRUE(state.contains("engines"));
    ASSERT_TRUE(state["engines"].is_object());
    ASSERT_TRUE(state["engines"].empty());
}

TEST_F(ApiTest, LoadStateFromZeroJobsAndZeroEnginesFile) {
    // 1. Clear the in-memory databases
    clear_db();

    // 2. Save the state (this will create a file with zero jobs and engines)
    save_state();

    // 3. Clear the in-memory databases again to simulate a fresh start
    clear_db();

    // 4. Load the state from the file
    load_state();

    // 5. Verify that the in-memory databases are empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.empty());
        ASSERT_TRUE(engines_db.empty());
    }
}

TEST_F(ApiTest, JobIdGenerationIsUnique) {
    // 1. Generate a large number of job IDs and check for uniqueness
    const int num_jobs = 10000; // Generate 10,000 job IDs
    std::set<std::string> job_ids;
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };

    for (int i = 0; i < num_jobs; ++i) {
        nlohmann::json job_payload = {
            {"source_url", "http://example.com/video_" + std::to_string(i) + ".mp4"},
            {"target_codec", "h264"}
        };
        auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
        ASSERT_EQ(res_submit->status, 200);
        std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
        
        // Check if the job ID already exists in the set
        ASSERT_TRUE(job_ids.find(job_id) == job_ids.end()) << "Duplicate job ID found: " << job_id;
        job_ids.insert(job_id);
    }
    // Verify that the number of unique job IDs matches the number of jobs generated
    ASSERT_EQ(job_ids.size(), num_jobs);
}

TEST_F(ApiTest, JobsDbCanBeClearedAndPrePopulated) {
    // 1. Add some initial jobs
    nlohmann::json job1 = {{"job_id", "job_initial_1"}, {"source_url", "url_initial_1"}, {"status", "pending"}};
    nlohmann::json job2 = {{"job_id", "job_initial_2"}, {"source_url", "url_initial_2"}, {"status", "completed"}};
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db["job_initial_1"] = job1;
        jobs_db["job_initial_2"] = job2;
    }

    // Verify initial state
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_EQ(jobs_db.size(), 2);
        ASSERT_TRUE(jobs_db.contains("job_initial_1"));
        ASSERT_TRUE(jobs_db.contains("job_initial_2"));
    }

    // 2. Clear the database
    clear_db();

    // Verify database is empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.empty());
    }

    // 3. Pre-populate with new jobs
    nlohmann::json job_new_1 = {{"job_id", "job_new_1"}, {"source_url", "url_new_1"}, {"status", "pending"}};
    nlohmann::json job_new_2 = {{"job_id", "job_new_2"}, {"source_url", "url_new_2"}, {"status", "assigned"}};
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        jobs_db["job_new_1"] = job_new_1;
        jobs_db["job_new_2"] = job_new_2;
    }

    // Verify new state
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_EQ(jobs_db.size(), 2);
        ASSERT_TRUE(jobs_db.contains("job_new_1"));
        ASSERT_TRUE(jobs_db.contains("job_new_2"));
        ASSERT_FALSE(jobs_db.contains("job_initial_1"));
    }
}

TEST_F(ApiTest, EnginesDbCanBeClearedAndPrePopulated) {
    // 1. Add some initial engines
    nlohmann::json engine1 = {{"engine_id", "engine_initial_1"}, {"engine_type", "transcoder"}, {"status", "idle"}};
    nlohmann::json engine2 = {{"engine_id", "engine_initial_2"}, {"engine_type", "transcoder"}, {"status", "busy"}};
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        engines_db["engine_initial_1"] = engine1;
        engines_db["engine_initial_2"] = engine2;
    }

    // Verify initial state
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_EQ(engines_db.size(), 2);
        ASSERT_TRUE(engines_db.contains("engine_initial_1"));
        ASSERT_TRUE(engines_db.contains("engine_initial_2"));
    }

    // 2. Clear the database
    clear_db();

    // Verify database is empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(engines_db.empty());
    }

    // 3. Pre-populate with new engines
    nlohmann::json engine_new_1 = {{"engine_id", "engine_new_1"}, {"engine_type", "transcoder"}, {"status", "idle"}};
    nlohmann::json engine_new_2 = {{"engine_id", "engine_new_2"}, {"engine_type", "transcoder"}, {"status", "busy"}};
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        engines_db["engine_new_1"] = engine_new_1;
        engines_db["engine_new_2"] = engine_new_2;
    }

    // Verify new state
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_EQ(engines_db.size(), 2);
        ASSERT_TRUE(engines_db.contains("engine_new_1"));
        ASSERT_TRUE(engines_db.contains("engine_new_2"));
        ASSERT_FALSE(engines_db.contains("engine_initial_1"));
    }
}

TEST_F(ApiTest, PersistentStorageFileCanBePointedToTemporaryFile) {
    // 1. Set a temporary file path for persistent storage
    std::string original_persistent_storage_file = PERSISTENT_STORAGE_FILE;
    std::string temp_storage_file = "temp_persistent_storage_test.json";
    PERSISTENT_STORAGE_FILE = temp_storage_file;

    // Ensure the temporary file does not exist initially
    std::remove(temp_storage_file.c_str());

    // 2. Create a job and save the state
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video_temp_file.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    save_state();

    // 3. Verify that the state was saved to the temporary file
    std::ifstream ifs(temp_storage_file);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json state = nlohmann::json::parse(ifs);
    ifs.close();

    ASSERT_TRUE(state.contains("jobs"));
    ASSERT_TRUE(state["jobs"].contains(job_id));
    ASSERT_EQ(state["jobs"][job_id]["source_url"], "http://example.com/video_temp_file.mp4");

    // 4. Clean up the temporary file and restore original path
    std::remove(temp_storage_file.c_str());
    PERSISTENT_STORAGE_FILE = original_persistent_storage_file;
}

TEST_F(ApiTest, SaveStateCanBeMocked) {
    // Enable mocking of save_state
    mock_save_state_enabled = true;
    save_state_call_count = 0; // Reset counter

    // Perform an action that would normally trigger save_state
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video_mock.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers admin_headers = {
        {"Authorization", "some_token"},
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", admin_headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);

    // Verify that save_state was called (mocked version)
    ASSERT_EQ(save_state_call_count, 1);

    // Verify that no file was actually written (by checking if the file exists)
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_FALSE(ifs.is_open());

    // Disable mocking after the test
    mock_save_state_enabled = false;
}

TEST_F(ApiTest, LoadStateCanBeMocked) {
    // 1. Prepare mocked data
    nlohmann::json mocked_jobs = {
        {"mock_job_1", {{"job_id", "mock_job_1"}, {"source_url", "http://mock.com/video1.mp4"}, {"status", "pending"}}},
        {"mock_job_2", {{"job_id", "mock_job_2"}, {"source_url", "http://mock.com/video2.mp4"}, {"status", "assigned"}}}
    };
    nlohmann::json mocked_engines = {
        {"mock_engine_1", {{"engine_id", "mock_engine_1"}, {"engine_type", "transcoder"}, {"status", "idle"}}},
        {"mock_engine_2", {{"engine_id", "mock_engine_2"}, {"engine_type", "transcoder"}, {"status", "busy"}}}
    };
    mock_load_state_data["jobs"] = mocked_jobs;
    mock_load_state_data["engines"] = mocked_engines;

    // 2. Enable mocking of load_state
    mock_load_state_enabled = true;

    // 3. Clear current in-memory state and call load_state
    clear_db();
    load_state();

    // 4. Verify that the mocked data was loaded into the in-memory databases
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_EQ(jobs_db.size(), 2);
        ASSERT_TRUE(jobs_db.contains("mock_job_1"));
        ASSERT_TRUE(jobs_db.contains("mock_job_2"));
        ASSERT_EQ(jobs_db["mock_job_1"]["source_url"], "http://mock.com/video1.mp4");
        ASSERT_EQ(jobs_db["mock_job_2"]["status"], "assigned");

        ASSERT_EQ(engines_db.size(), 2);
        ASSERT_TRUE(engines_db.contains("mock_engine_1"));
        ASSERT_TRUE(engines_db.contains("mock_engine_2"));
        ASSERT_EQ(engines_db["mock_engine_1"]["status"], "idle");
        ASSERT_EQ(engines_db["mock_engine_2"]["engine_type"], "transcoder");
    }

    // 5. Disable mocking after the test
    mock_load_state_enabled = false;
    mock_load_state_data.clear();
}

TEST_F(ApiTest, SaveStateProducesFormattedJson) {
    // Test 97: save_state produces a well-formatted (indented) JSON file.
    
    // 1. Create a job and engine
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    // 2. Save state
    save_state();
    
    // 3. Read the file as raw text and verify it's indented
    std::ifstream ifs(PERSISTENT_STORAGE_FILE);
    ASSERT_TRUE(ifs.is_open());
    std::string file_content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();
    
    // 4. Verify the JSON is formatted with indentation (contains newlines and spaces)
    ASSERT_TRUE(file_content.find('\n') != std::string::npos); // Contains newlines
    ASSERT_TRUE(file_content.find("  \"jobs\"") != std::string::npos || file_content.find("    \"jobs\"") != std::string::npos); // Contains indented keys
}

TEST_F(ApiTest, LoadStateHandlesOnlyEnginesKey) {
    // Test 99: load_state handles a file with only an "engines" key.
    
    // 1. Create a state file with only an "engines" key
    std::string temp_storage_file = "temp_state.json";
    PERSISTENT_STORAGE_FILE = temp_storage_file;
    nlohmann::json state_with_only_engines = {
        {"engines", {{"engine1", {{"engine_id", "engine1"}, {"status", "idle"}}}}}
    };
    std::ofstream ofs(temp_storage_file);
    ofs << state_with_only_engines.dump(4);
    ofs.close();
    
    // 2. Load the state
    load_state();
    
    // 3. Verify that engines are loaded and jobs are empty
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(engines_db.contains("engine1"));
        ASSERT_EQ(engines_db["engine1"]["engine_id"], "engine1");
        ASSERT_EQ(engines_db["engine1"]["status"], "idle");
        ASSERT_TRUE(jobs_db.empty());
    }
    
    // 4. Clean up the temporary file
    std::remove(temp_storage_file.c_str());
}

TEST_F(ApiTest, SaveStateHandlesSpecialCharacters) {
    // Test 100: save_state correctly handles special characters in job/engine data.
    
    // 1. Create a job with special characters
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video with spaces & symbols!@#.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // 2. Create an engine with special characters
    nlohmann::json engine_payload = {
        {"engine_id", "engine-with-special-chars-αβγ"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    // 3. Save state
    save_state();
    
    // 4. Load state to verify round-trip correctness
    load_state();
    
    // 5. Verify special characters are preserved
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ASSERT_TRUE(jobs_db.contains(job_id));
        ASSERT_EQ(jobs_db[job_id]["source_url"], "http://example.com/video with spaces & symbols!@#.mp4");
        
        ASSERT_TRUE(engines_db.contains("engine-with-special-chars-αβγ"));
        ASSERT_EQ(engines_db["engine-with-special-chars-αβγ"]["engine_id"], "engine-with-special-chars-αβγ");
    }
}
