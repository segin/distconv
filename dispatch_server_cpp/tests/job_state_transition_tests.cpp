#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "../dispatch_server_core.h"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>

#include "test_common.h"

using namespace distconv::DispatchServer;

TEST_F(ApiTest, JobIsPendingAfterSubmission) {
    // Test 51: Job is `pending` after submission.
    
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    
    auto res = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    
    nlohmann::json response = nlohmann::json::parse(res->body);
    ASSERT_EQ(response["status"], "pending");
}

TEST_F(ApiTest, JobBecomesAssignedAfterAssignment) {
    // Test 52: Job becomes `assigned` after `POST /assign_job/`.
    
    // 1. Create a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // 2. Register an idle engine
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    // 3. Assign the job
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // 4. Verify job status is now assigned
    auto res_get = client->Get(("/jobs/" + job_id).c_str(), headers);
    ASSERT_EQ(res_get->status, 200);
    nlohmann::json job_status = nlohmann::json::parse(res_get->body);
    ASSERT_EQ(job_status["status"], "assigned");
    ASSERT_EQ(job_status["assigned_engine"], "engine-123");
}

TEST_F(ApiTest, JobBecomesCompletedAfterCompletion) {
    // Test 53: Job becomes `completed` after `POST /jobs/{job_id}/complete`.
    
    // 1. Create and assign a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // 2. Register an idle engine and assign job
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // 3. Complete the job
    nlohmann::json complete_payload = {
        {"output_url", "http://example.com/video_out.mp4"}
    };
    auto res_complete = client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, complete_payload.dump(), "application/json");
    ASSERT_EQ(res_complete->status, 200);
    
    // 4. Verify job status is now completed
    auto res_get = client->Get(("/jobs/" + job_id).c_str(), headers);
    ASSERT_EQ(res_get->status, 200);
    nlohmann::json job_status = nlohmann::json::parse(res_get->body);
    ASSERT_EQ(job_status["status"], "completed");
    ASSERT_EQ(job_status["output_url"], "http://example.com/video_out.mp4");
}

TEST_F(ApiTest, JobBecomesPendingAgainAfterFailureWithRetriesAvailable) {
    // Test 54: Job becomes `pending` again after `POST /jobs/{job_id}/fail` if retries are available.
    
    // 1. Create a job with max_retries > 1
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", 2}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // 2. Register an idle engine and assign job
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // 3. Fail the job
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);
    
    // 4. Verify job status is back to pending (since retries < max_retries)
    auto res_get = client->Get(("/jobs/" + job_id).c_str(), headers);
    ASSERT_EQ(res_get->status, 200);
    nlohmann::json job_status = nlohmann::json::parse(res_get->body);
    ASSERT_EQ(job_status["status"], "pending");
    ASSERT_EQ(job_status["retries"], 1);
    ASSERT_EQ(job_status["max_retries"], 2);
}

TEST_F(ApiTest, JobBecomesFailedPermanentlyAfterFailureWithNoRetriesLeft) {
    // Test 55: Job becomes `failed_permanently` after `POST /jobs/{job_id}/fail` if retries are exhausted.
    
    // 1. Create a job with max_retries = 1
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", 1}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // 2. Register an idle engine and assign job
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // 3. Fail the job
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);
    
    // 4. Verify job status is failed_permanently (since retries >= max_retries)
    auto res_get = client->Get(("/jobs/" + job_id).c_str(), headers);
    ASSERT_EQ(res_get->status, 200);
    nlohmann::json job_status = nlohmann::json::parse(res_get->body);
    ASSERT_EQ(job_status["status"], "failed_permanently");
    ASSERT_EQ(job_status["retries"], 1);
    ASSERT_EQ(job_status["max_retries"], 1);
}

TEST_F(ApiTest, CompletedJobCannotBeMarkedAsFailed) {
    // Test 56: A `completed` job cannot be marked as failed.
    
    // 1. Create, assign and complete a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // 2. Register an idle engine and assign job
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // 3. Complete the job
    nlohmann::json complete_payload = {
        {"output_url", "http://example.com/video_out.mp4"}
    };
    auto res_complete = client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, complete_payload.dump(), "application/json");
    ASSERT_EQ(res_complete->status, 200);
    
    // 4. Try to fail the completed job (should fail)
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 400);
    ASSERT_EQ(res_fail->body, "Bad Request: Job is already in a final state.");
}

TEST_F(ApiTest, FailedPermanentlyJobCannotBeMarkedAsFailedAgain) {
    // Test 57: A `failed_permanently` job cannot be marked as failed again.
    
    // 1. Create a job with max_retries = 0 (so it fails permanently immediately)
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", 0}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // 2. Register an idle engine and assign job
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // 3. Fail the job (should become failed_permanently since max_retries = 0)
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);
    
    // 4. Verify job is failed_permanently
    auto res_get = client->Get(("/jobs/" + job_id).c_str(), headers);
    ASSERT_EQ(res_get->status, 200);
    nlohmann::json job_status = nlohmann::json::parse(res_get->body);
    ASSERT_EQ(job_status["status"], "failed_permanently");
    
    // 5. Try to fail the job again (should fail)
    auto res_fail_again = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail_again->status, 400);
    ASSERT_EQ(res_fail_again->body, "Bad Request: Job is already in a final state.");
}

TEST_F(ApiTest, CompletedJobIsNotReturnedByAssignJob) {
    // Test 58: A `completed` job is not returned by `POST /assign_job/`.
    
    // 1. Create, assign and complete a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // 2. Register an idle engine and assign job
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // 3. Complete the job
    nlohmann::json complete_payload = {
        {"output_url", "http://example.com/video_out.mp4"}
    };
    auto res_complete = client->Post(("/jobs/" + job_id + "/complete").c_str(), headers, complete_payload.dump(), "application/json");
    ASSERT_EQ(res_complete->status, 200);
    
    // 4. Register another idle engine and try to assign a job (should get no jobs)
    nlohmann::json engine2_payload = {
        {"engine_id", "engine-456"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat2 = client->Post("/engines/heartbeat", headers, engine2_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat2->status, 200);
    
    nlohmann::json assign2_payload = {
        {"engine_id", "engine-456"}
    };
    auto res_assign2 = client->Post("/assign_job/", headers, assign2_payload.dump(), "application/json");
    ASSERT_EQ(res_assign2->status, 204); // No Content - completed job not assignable
}

TEST_F(ApiTest, FailedPermanentlyJobIsNotReturnedByAssignJob) {
    // Test 59: A `failed_permanently` job is not returned by `POST /assign_job/`.
    
    // 1. Create a job with max_retries = 0 (so it fails permanently immediately)
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"},
        {"max_retries", 0}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // 2. Register an idle engine and assign job
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // 3. Fail the job (should become failed_permanently since max_retries = 0)
    nlohmann::json fail_payload = {
        {"error_message", "Transcoding failed"}
    };
    auto res_fail = client->Post(("/jobs/" + job_id + "/fail").c_str(), headers, fail_payload.dump(), "application/json");
    ASSERT_EQ(res_fail->status, 200);
    
    // 4. Register another idle engine and try to assign a job (should get no jobs)
    nlohmann::json engine2_payload = {
        {"engine_id", "engine-456"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat2 = client->Post("/engines/heartbeat", headers, engine2_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat2->status, 200);
    
    nlohmann::json assign2_payload = {
        {"engine_id", "engine-456"}
    };
    auto res_assign2 = client->Post("/assign_job/", headers, assign2_payload.dump(), "application/json");
    ASSERT_EQ(res_assign2->status, 204); // No Content - failed_permanently job not assignable
}

TEST_F(ApiTest, AssignedJobIsNotReturnedByAssignJob) {
    // Test 60: An `assigned` job is not returned by `POST /assign_job/`.
    
    // 1. Create a job
    nlohmann::json job_payload = {
        {"source_url", "http://example.com/video.mp4"},
        {"target_codec", "h264"}
    };
    httplib::Headers headers = {
        {"X-API-Key", api_key}
    };
    auto res_submit = client->Post("/jobs/", headers, job_payload.dump(), "application/json");
    ASSERT_EQ(res_submit->status, 200);
    std::string job_id = nlohmann::json::parse(res_submit->body)["job_id"];
    
    // 2. Register an idle engine and assign job
    nlohmann::json engine_payload = {
        {"engine_id", "engine-123"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat = client->Post("/engines/heartbeat", headers, engine_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat->status, 200);
    
    nlohmann::json assign_payload = {
        {"engine_id", "engine-123"}
    };
    auto res_assign = client->Post("/assign_job/", headers, assign_payload.dump(), "application/json");
    ASSERT_EQ(res_assign->status, 200);
    
    // 3. Register another idle engine and try to assign a job (should get no jobs)
    nlohmann::json engine2_payload = {
        {"engine_id", "engine-456"},
        {"engine_type", "transcoder"},
        {"supported_codecs", {"h264", "vp9"}},
        {"status", "idle"},
        {"benchmark_time", 100.0}
    };
    auto res_heartbeat2 = client->Post("/engines/heartbeat", headers, engine2_payload.dump(), "application/json");
    ASSERT_EQ(res_heartbeat2->status, 200);
    
    nlohmann::json assign2_payload = {
        {"engine_id", "engine-456"}
    };
    auto res_assign2 = client->Post("/assign_job/", headers, assign2_payload.dump(), "application/json");
    ASSERT_EQ(res_assign2->status, 204); // No Content - assigned job not re-assignable
}
