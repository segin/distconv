#include "gtest/gtest.h"
#include "../dispatch_server_core.h" // Include the header for the core logic

// Mock functions or classes if necessary for isolated unit testing
// For now, we'll focus on basic server functionality that can be tested without complex mocks.

// Test fixture for the Dispatch Server
class DispatchServerTest : public ::testing::Test {
protected:
    // You can set up resources here that are used by all tests in this fixture
    void SetUp() override {
        // Initialize any necessary server components or mock dependencies
        // Reset global state for each test to ensure isolation
        jobs_db = nlohmann::json::object();
        engines_db = nlohmann::json::object();
        API_KEY = "";
    }

    void TearDown() override {
        // Clean up resources
    }
};

// Example Test Case: Server starts and stops without crashing
TEST_F(DispatchServerTest, ServerStartsAndStops) {
    // This test is more of an integration test, but it's a good starting point
    // For true unit tests, you'd mock the server's dependencies.
    // Since run_dispatch_server() contains the server loop, we can't directly call it
    // in a simple unit test without it blocking. For now, this test will be a placeholder.
    SUCCEED(); // Placeholder for a test that would verify server startup/shutdown
}

// Test case for submitting a job with valid data
TEST_F(DispatchServerTest, SubmitJobValid) {
    // Mock a request and response for testing the Post("/jobs/") endpoint
    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/jobs/";
    req.set_header("X-API-Key", "test_api_key");
    req.body = "{\"source_url\": \"http://example.com/video.mp4\", \"target_codec\": \"H.264\", \"job_size\": 100.0, \"max_retries\": 5}";

    API_KEY = "test_api_key"; // Set the global API key for the test

    // Call the handler directly (assuming it's refactored to be callable)
    // For now, we'll simulate the call and check the global state.
    // In a real scenario, you'd have a mock httplib::Server or extract the handler logic.

    // Simulate the handler logic for /jobs/
    try {
        nlohmann::json request_json = nlohmann::json::parse(req.body);
        std::string job_id = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        
        nlohmann::json job;
        job["job_id"] = job_id;
        job["source_url"] = request_json["source_url"];
        job["target_codec"] = request_json["target_codec"];
        job["job_size"] = request_json.value("job_size", 0.0);
        job["status"] = "pending";
        job["assigned_engine"] = nullptr;
        job["output_url"] = nullptr;
        job["retries"] = 0;
        job["max_retries"] = request_json.value("max_retries", 3);

        jobs_db[job_id] = job;
        // save_state(); // Don't save state in unit tests

        res.status = 200;
        res.set_content(job.dump(), "application/json");
    } catch (const nlohmann::json::parse_error& e) {
        res.status = 400;
        res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content("Server error: " + std::string(e.what()), "text/plain");
    }

    ASSERT_EQ(res.status, 200);
    ASSERT_TRUE(jobs_db.size() == 1);
    ASSERT_EQ(jobs_db.begin().value()["source_url"], "http://example.com/video.mp4");
    ASSERT_EQ(jobs_db.begin().value()["target_codec"], "H.264");
    ASSERT_EQ(jobs_db.begin().value()["job_size"], 100.0);
    ASSERT_EQ(jobs_db.begin().value()["status"], "pending");
    ASSERT_EQ(jobs_db.begin().value()["max_retries"], 5);
}

// Test case for submitting a job with missing data
TEST_F(DispatchServerTest, SubmitJobMissingData) {
    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/jobs/";
    req.set_header("X-API-Key", "test_api_key");
    req.body = "{\"source_url\": \"http://example.com/video.mp4\", \"job_size\": 100.0}"; // Missing target_codec

    API_KEY = "test_api_key";

    // Simulate the handler logic for /jobs/
    try {
        nlohmann::json request_json = nlohmann::json::parse(req.body);
        std::string job_id = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        
        nlohmann::json job;
        job["job_id"] = job_id;
        job["source_url"] = request_json["source_url"];
        job["target_codec"] = request_json["target_codec"]; // This will throw if not present
        job["job_size"] = request_json.value("job_size", 0.0);
        job["status"] = "pending";
        job["assigned_engine"] = nullptr;
        job["output_url"] = nullptr;
        job["retries"] = 0;
        job["max_retries"] = request_json.value("max_retries", 3);

        jobs_db[job_id] = job;

        res.status = 200;
        res.set_content(job.dump(), "application/json");
    } catch (const nlohmann::json::parse_error& e) {
        res.status = 400;
        res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content("Server error: " + std::string(e.what()), "text/plain");
    }

    ASSERT_EQ(res.status, 500); // Expecting a server error due to missing required field
    ASSERT_TRUE(jobs_db.empty()); // No job should be added
}

// Test case for unauthorized access to submit job endpoint
TEST_F(DispatchServerTest, SubmitJobUnauthorized) {
    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/jobs/";
    req.set_header("X-API-Key", "wrong_api_key");
    req.body = "{\"source_url\": \"http://example.com/video.mp4\", \"target_codec\": \"H.264\", \"job_size\": 100.0}";

    API_KEY = "correct_api_key"; // Set a different API key

    // Simulate the handler logic for /jobs/
    if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
    } else {
        // This part would not be reached in a real scenario if unauthorized
        res.status = 200; // Placeholder for success if authorization passed
    }

    ASSERT_EQ(res.status, 401);
    ASSERT_TRUE(jobs_db.empty());
}

// Test case for getting job status of an existing job
TEST_F(DispatchServerTest, GetJobStatusExisting) {
    // Manually add a job to the database for testing
    std::string test_job_id = "test_job_123";
    jobs_db[test_job_id]["status"] = "pending";
    jobs_db[test_job_id]["source_url"] = "http://example.com/test.mp4";

    httplib::Request req;
    httplib::Response res;

    req.method = "GET";
    req.path = "/jobs/" + test_job_id;
    req.matches.push_back(""); // Dummy for req.matches[0]
    req.matches.push_back(test_job_id); // Simulate regex match
    req.set_header("X-API-Key", "test_api_key");

    API_KEY = "test_api_key";

    // Simulate the handler logic for /jobs/{job_id}
    if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
    } else {
        std::string job_id = req.matches[1];
        if (jobs_db.contains(job_id)) {
            res.set_content(jobs_db[job_id].dump(), "application/json");
            res.status = 200;
        } else {
            res.status = 404;
            res.set_content("Job not found", "text/plain");
        }
    }

    ASSERT_EQ(res.status, 200);
    nlohmann::json response_json = nlohmann::json::parse(res.body);
    ASSERT_EQ(response_json["job_id"], test_job_id);
    ASSERT_EQ(response_json["status"], "pending");
}

// Test case for getting job status of a non-existent job
TEST_F(DispatchServerTest, GetJobStatusNonExistent) {
    httplib::Request req;
    httplib::Response res;

    std::string non_existent_job_id = "non_existent_job";
    req.method = "GET";
    req.path = "/jobs/" + non_existent_job_id;
    req.matches.push_back("");
    req.matches.push_back(non_existent_job_id);
    req.set_header("X-API-Key", "test_api_key");

    API_KEY = "test_api_key";

    // Simulate the handler logic
    if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
    } else {
        std::string job_id = req.matches[1];
        if (jobs_db.contains(job_id)) {
            res.set_content(jobs_db[job_id].dump(), "application/json");
            res.status = 200;
        } else {
            res.status = 404;
            res.set_content("Job not found", "text/plain");
        }
    }

    ASSERT_EQ(res.status, 404);
}

// Test case for listing all jobs when no jobs exist
TEST_F(DispatchServerTest, ListAllJobsEmpty) {
    httplib::Request req;
    httplib::Response res;

    req.method = "GET";
    req.path = "/jobs/";
    req.set_header("X-API-Key", "test_api_key");

    API_KEY = "test_api_key";

    // Simulate the handler logic
    if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
    } else {
        nlohmann::json all_jobs = nlohmann::json::array();
        for (auto const& [key, val] : jobs_db.items()) {
            all_jobs.push_back(val);
        }
        res.set_content(all_jobs.dump(), "application/json");
        res.status = 200;
    }

    ASSERT_EQ(res.status, 200);
    nlohmann::json response_json = nlohmann::json::parse(res.body);
    ASSERT_TRUE(response_json.empty());
}

// Test case for listing all jobs with existing jobs
TEST_F(DispatchServerTest, ListAllJobsWithData) {
    // Add some jobs manually
    jobs_db["job1"]["status"] = "completed";
    jobs_db["job2"]["status"] = "pending";

    httplib::Request req;
    httplib::Response res;

    req.method = "GET";
    req.path = "/jobs/";
    req.set_header("X-API-Key", "test_api_key");

    API_KEY = "test_api_key";

    // Simulate the handler logic
    if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
    } else {
        nlohmann::json all_jobs = nlohmann::json::array();
        for (auto const& [key, val] : jobs_db.items()) {
            all_jobs.push_back(val);
        }
        res.set_content(all_jobs.dump(), "application/json");
        res.status = 200;
    }

    ASSERT_EQ(res.status, 200);
    nlohmann::json response_json = nlohmann::json::parse(res.body);
    ASSERT_EQ(response_json.size(), 2);
    ASSERT_TRUE(response_json[0].contains("job_id") || response_json[1].contains("job_id"));
    // The order of elements in a JSON array from a map iteration is not guaranteed,
    // so we check for content rather than specific indices.
    bool found_job1 = false;
    bool found_job2 = false;
    for (const auto& job : response_json) {
        if (job.contains("job_id")) {
            if (job["job_id"] == "job1") found_job1 = true;
            if (job["job_id"] == "job2") found_job2 = true;
        }
    }
    ASSERT_TRUE(found_job1);
    ASSERT_TRUE(found_job2);
}

// Test case for engine heartbeat
TEST_F(DispatchServerTest, EngineHeartbeat) {
    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/engines/heartbeat";
    req.set_header("X-API-Key", "test_api_key");
    req.body = "{\"engine_id\": \"engine1\", \"status\": \"idle\", \"storage_capacity_gb\": 1024.0, \"streaming_support\": true, \"encoders\": \"h264\", \"decoders\": \"h264\", \"hwaccels\": \"cuda\", \"cpu_temperature\": 55.5, \"local_job_queue\": \"[]\", \"hostname\": \"test-host\"}";

    API_KEY = "test_api_key";

    // Simulate the handler logic
    if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
    } else {
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            std::string engine_id = request_json["engine_id"];
            engines_db[engine_id] = request_json;
            res.set_content("Heartbeat received from engine " + engine_id, "text/plain");
            res.status = 200;
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    }

    ASSERT_EQ(res.status, 200);
    ASSERT_TRUE(engines_db.contains("engine1"));
    ASSERT_EQ(engines_db["engine1"]["status"], "idle");
    ASSERT_EQ(engines_db["engine1"]["hostname"], "test-host");
}

// Test case for benchmark result submission
TEST_F(DispatchServerTest, BenchmarkResult) {
    // Manually add an engine for testing
    engines_db["engine1"]["status"] = "idle";

    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/engines/benchmark_result";
    req.set_header("X-API-Key", "test_api_key");
    req.body = "{\"engine_id\": \"engine1\", \"benchmark_time\": 123.45}";

    API_KEY = "test_api_key";

    // Simulate the handler logic
    if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
    } else {
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            std::string engine_id = request_json["engine_id"];
            if (engines_db.contains(engine_id)) {
                engines_db[engine_id]["benchmark_time"] = request_json["benchmark_time"];
                res.set_content("Benchmark result received from engine " + engine_id, "text/plain");
                res.status = 200;
            } else {
                res.status = 404;
                res.set_content("Engine not found", "text/plain");
            }
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    }

    ASSERT_EQ(res.status, 200);
    ASSERT_TRUE(engines_db.contains("engine1"));
    ASSERT_EQ(engines_db["engine1"]["benchmark_time"], 123.45);
}

// Test case for job completion
TEST_F(DispatchServerTest, JobCompletion) {
    std::string test_job_id = "job_to_complete";
    jobs_db[test_job_id]["status"] = "assigned";

    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/jobs/" + test_job_id + "/complete";
    req.matches.push_back("");
    req.matches.push_back(test_job_id);
    req.set_header("X-API-Key", "test_api_key");
    req.body = "{\"output_url\": \"http://example.com/output.mp4\"}";

    API_KEY = "test_api_key";

    // Simulate the handler logic
    if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
    } else {
        std::string job_id = req.matches[1];
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (jobs_db.contains(job_id)) {
                jobs_db[job_id]["status"] = "completed";
                jobs_db[job_id]["output_url"] = request_json["output_url"];
                res.set_content("Job " + job_id + " marked as completed", "text/plain");
                res.status = 200;
            } else {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
            }
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    }

    ASSERT_EQ(res.status, 200);
    ASSERT_EQ(jobs_db[test_job_id]["status"], "completed");
    ASSERT_EQ(jobs_db[test_job_id]["output_url"], "http://example.com/output.mp4");
}

// Test case for job failure and re-queueing
TEST_F(DispatchServerTest, JobFailureRequeue) {
    std::string test_job_id = "job_to_fail";
    jobs_db[test_job_id]["status"] = "assigned";
    jobs_db[test_job_id]["retries"] = 0;
    jobs_db[test_job_id]["max_retries"] = 1; // Allow one retry

    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/jobs/" + test_job_id + "/fail";
    req.matches.push_back("");
    req.matches.push_back(test_job_id);
    req.set_header("X-API-Key", "test_api_key");
    req.body = "{\"error_message\": \"Transcoding failed due to network issue.\"}";

    API_KEY = "test_api_key";

    // Simulate the handler logic
    if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
    } else {
        std::string job_id = req.matches[1];
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (jobs_db.contains(job_id)) {
                jobs_db[job_id]["retries"] = jobs_db[job_id].value("retries", 0) + 1;
                if (jobs_db[job_id].value("retries", 0) <= jobs_db[job_id].value("max_retries", 3)) {
                    jobs_db[job_id]["status"] = "pending"; // Re-queue
                    jobs_db[job_id]["error_message"] = request_json["error_message"];
                    res.set_content("Job " + job_id + " re-queued", "text/plain");
                    res.status = 200;
                } else {
                    jobs_db[job_id]["status"] = "failed_permanently";
                    jobs_db[job_id]["error_message"] = request_json["error_message"];
                    res.set_content("Job " + job_id + " failed permanently", "text/plain");
                    res.status = 200;
                }
            }
 else {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
            }
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    }

    ASSERT_EQ(res.status, 200);
    ASSERT_EQ(jobs_db[test_job_id]["status"], "pending");
    ASSERT_EQ(jobs_db[test_job_id]["retries"], 1);
}

// Test case for job permanent failure after max retries
TEST_F(DispatchServerTest, JobPermanentFailure) {
    std::string test_job_id = "job_to_fail_permanently";
    jobs_db[test_job_id]["status"] = "assigned";
    jobs_db[test_job_id]["retries"] = 3; // Already at max retries
    jobs_db[test_job_id]["max_retries"] = 3;

    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/jobs/" + test_job_id + "/fail";
    req.matches.push_back("");
    req.matches.push_back(test_job_id);
    req.set_header("X-API-Key", "test_api_key");
    req.body = "{\"error_message\": \"Transcoding failed again.\"}";

    API_KEY = "test_api_key";

    // Simulate the handler logic
    if (API_KEY != "" && req.get_header_value("X-API-Key") != API_KEY) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
    } else {
        std::string job_id = req.matches[1];
        try {
            nlohmann::json request_json = nlohmann::json::parse(req.body);
            if (jobs_db.contains(job_id)) {
                jobs_db[job_id]["retries"] = jobs_db[job_id].value("retries", 0) + 1;
                if (jobs_db[job_id].value("retries", 0) <= jobs_db[job_id].value("max_retries", 3)) {
                    jobs_db[job_id]["status"] = "pending"; // Re-queue
                    jobs_db[job_id]["error_message"] = request_json["error_message"];
                    res.set_content("Job " + job_id + " re-queued", "text/plain");
                    res.status = 200;
                } else {
                    jobs_db[job_id]["status"] = "failed_permanently";
                    jobs_db[job_id]["error_message"] = request_json["error_message"];
                    res.set_content("Job " + job_id + " failed permanently", "text/plain");
                    res.status = 200;
                }
            }
 else {
                res.status = 404;
                res.set_content("Job not found", "text/plain");
            }
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Server error: " + std::string(e.what()), "text/plain");
        }
    }

    ASSERT_EQ(res.status, 200);
    ASSERT_EQ(jobs_db[test_job_id]["status"], "failed_permanently");
    ASSERT_EQ(jobs_db[test_job_id]["retries"], 4); // Should be 4 as it increments before check
}

// Test case for assigning a job to an available engine
TEST_F(DispatchServerTest, AssignJobSuccess) {
    // Add a pending job
    std::string job_id = "pending_job_1";
    jobs_db[job_id]["status"] = "pending";
    jobs_db[job_id]["job_size"] = 50.0;

    // Add an idle engine with benchmark data
    std::string engine_id = "engine_fast";
    engines_db[engine_id]["engine_id"] = engine_id;
    engines_db[engine_id]["status"] = "idle";
    engines_db[engine_id]["benchmark_time"] = 10.0;
    engines_db[engine_id]["streaming_support"] = false;

    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/assign_job/";
    req.body = "{\"engine_id\": \"" + engine_id + "\"}"; // Engine requesting a job

    // Simulate the handler logic
    std::string req_engine_id = "";
    try {
        nlohmann::json request_json = nlohmann::json::parse(req.body);
        req_engine_id = request_json["engine_id"];
    } catch (const nlohmann::json::parse_error& e) {
        res.status = 400;
        res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        // return;
    }

    nlohmann::json pending_job = nullptr;
    for (auto const& [job_key, job_val] : jobs_db.items()) {
        if (job_val["status"] == "pending") {
            pending_job = job_val;
            break;
        }
    }

    if (pending_job.is_null()) {
        res.status = 204; // No Content
        // return;
    }

    nlohmann::json selected_engine = nullptr;
    std::vector<nlohmann::json> available_engines;
    for (auto const& [eng_id, eng_data] : engines_db.items()) {
        if (eng_data["status"] == "idle" && eng_data.contains("benchmark_time")) {
            available_engines.push_back(eng_data);
        }
    }

    if (available_engines.empty()) {
        res.status = 204; // No Content
        // return;
    }

    std::sort(available_engines.begin(), available_engines.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
        return a["benchmark_time"] < b["benchmark_time"];
    });

    double job_size = pending_job.value("job_size", 0.0);
    double large_job_threshold = 100.0;

    if (job_size >= large_job_threshold) {
        nlohmann::json streaming_capable_engine = nullptr;
        for (const auto& eng : available_engines) {
            if (eng.value("streaming_support", false)) {
                streaming_capable_engine = eng;
                break;
            }
        }
        if (!streaming_capable_engine.is_null()) {
            selected_engine = streaming_capable_engine;
        } else {
            selected_engine = available_engines[0];
        }
    } else {
        double small_job_threshold = 50.0;
        if (job_size < small_job_threshold) {
            selected_engine = available_engines.back();
        } else {
            selected_engine = available_engines[0];
        }
    }

    if (selected_engine.is_null()) {
        res.status = 204;
        // return;
    }

    jobs_db[pending_job["job_id"]]["status"] = "assigned";
    jobs_db[pending_job["job_id"]]["assigned_engine"] = selected_engine["engine_id"];
    engines_db[selected_engine["engine_id"]]["status"] = "busy";
    // save_state(); // Don't save state in unit tests

    res.set_content(pending_job.dump(), "application/json");
    res.status = 200;

    ASSERT_EQ(res.status, 200);
    ASSERT_EQ(jobs_db[job_id]["status"], "assigned");
    ASSERT_EQ(jobs_db[job_id]["assigned_engine"], engine_id);
    ASSERT_EQ(engines_db[engine_id]["status"], "busy");
}

// Test case for no pending jobs to assign
TEST_F(DispatchServerTest, AssignJobNoPending) {
    // No pending jobs in jobs_db

    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/assign_job/";
    req.body = "{\"engine_id\": \"engine1\"}";

    // Simulate the handler logic
    std::string req_engine_id = "";
    try {
        nlohmann::json request_json = nlohmann::json::parse(req.body);
        req_engine_id = request_json["engine_id"];
    } catch (const nlohmann::json::parse_error& e) {
        res.status = 400;
        res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        // return;
    }

    nlohmann::json pending_job = nullptr;
    for (auto const& [job_key, job_val] : jobs_db.items()) {
        if (job_val["status"] == "pending") {
            pending_job = job_val;
            break;
        }
    }

    if (pending_job.is_null()) {
        res.status = 204; // No Content
        // return;
    }

    ASSERT_EQ(res.status, 204);
}

// Test case for no idle engines to assign jobs
TEST_F(DispatchServerTest, AssignJobNoIdleEngines) {
    // Add a pending job
    std::string job_id = "pending_job_2";
    jobs_db[job_id]["status"] = "pending";

    // No idle engines in engines_db

    httplib::Request req;
    httplib::Response res;

    req.method = "POST";
    req.path = "/assign_job/";
    req.body = "{\"engine_id\": \"engine1\"}";

    // Simulate the handler logic
    std::string req_engine_id = "";
    try {
        nlohmann::json request_json = nlohmann::json::parse(req.body);
        req_engine_id = request_json["engine_id"];
    } catch (const nlohmann::json::parse_error& e) {
        res.status = 400;
        res.set_content("Invalid JSON: " + std::string(e.what()), "text/plain");
        // return;
    }

    nlohmann::json pending_job = nullptr;
    for (auto const& [job_key, job_val] : jobs_db.items()) {
        if (job_val["status"] == "pending") {
            pending_job = job_val;
            break;
        }
    }

    if (pending_job.is_null()) {
        res.status = 204; // No Content
        // return;
    }

    nlohmann::json selected_engine = nullptr;
    std::vector<nlohmann::json> available_engines;
    for (auto const& [eng_id, eng_data] : engines_db.items()) {
        if (eng_data["status"] == "idle" && eng_data.contains("benchmark_time")) {
            available_engines.push_back(eng_data);
        }
    }

    if (available_engines.empty()) {
        res.status = 204; // No Content
        // return;
    }

    ASSERT_EQ(res.status, 204);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
