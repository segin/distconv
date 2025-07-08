# Distributed Video Transcoding System - Development Context

This document provides essential context for Claude Code instances working on the distributed video transcoding system. This project implements a three-component architecture for distributed video transcoding with comprehensive testing frameworks.

## Project Overview

The system consists of three main components:
1. **Transcoding Engine** (C++) - Compute nodes that perform actual video transcoding using ffmpeg
2. **Dispatch Server** (C++) - Central coordinator managing job queues and engine assignments  
3. **Submission Client** (C++) - Interface for users to submit jobs and retrieve results

## Current Development Status

### Primary Focus: Dispatch Server Testing (C++)
- **Location**: `/home/segin/distconv/dispatch_server_cpp/`
- **Testing Framework**: Google Test (gtest)
- **Test Plan**: 150 total tests in `TESTING_TODO.md`
- **Progress**: Tests 1-47 completed, working on tests 48-150
- **Current Task**: Implementing test 48 (first test in section H: POST /assign_job/)

### Development Workflow
- **One test at a time**: Implement and verify one test before moving to the next
- **Git discipline**: Every completed test requires `git commit` and `git push`
- **Test tracking**: Mark completed tests with `[x]` in `TESTING_TODO.md`

## Key Files and Locations

### Core Implementation
- `dispatch_server_cpp/dispatch_server_core.cpp` - Main server implementation
- `dispatch_server_cpp/dispatch_server_core.h` - Server header/declarations
- `dispatch_server_cpp/CMakeLists.txt` - Build configuration

### Testing Infrastructure
- `dispatch_server_cpp/TESTING_TODO.md` - Master test plan (150 tests)
- `dispatch_server_cpp/tests/test_common.h` - Shared test utilities and ApiTest fixture
- `dispatch_server_cpp/tests/main.cpp` - Test runner entry point

### Test Files (by API sections)
- `tests/job_submission_tests.cpp` - Tests 1-15 (POST /jobs/)
- `tests/job_status_tests.cpp` - Tests 16-20 (GET /jobs/{job_id})
- `tests/list_jobs_tests.cpp` - Tests 21-25 (GET /jobs/)
- `tests/engine_heartbeat_tests.cpp` - Tests 26-32 (POST /engines/heartbeat)
- `tests/list_engines_tests.cpp` - Tests 33-37 (GET /engines/)
- `tests/job_completion_tests.cpp` - Tests 38-42 (POST /jobs/{job_id}/complete)
- `tests/jobs_status_update_tests.cpp` - Tests 43-47 (POST /jobs/{job_id}/fail)
- **Next**: Tests 48-50 need new file for POST /assign_job/

### Documentation
- `docs/protocol.md` - Comprehensive API protocol documentation
- `README.md` - Project roadmap and feature status
- `dispatch_server_state.json` - Runtime state persistence file

## API Architecture

### REST Endpoints (HTTP + JSON)
**Client API (Submission Client ↔ Dispatch Server):**
- `POST /jobs/` - Submit transcoding job
- `GET /jobs/{job_id}` - Get job status  
- `GET /jobs/` - List all jobs

**Engine API (Dispatch Server ↔ Transcoding Engines):**
- `POST /engines/heartbeat` - Engine registration/status updates
- `GET /engines/` - List all engines
- `POST /jobs/{job_id}/complete` - Mark job completed
- `POST /jobs/{job_id}/fail` - Mark job failed
- `POST /assign_job/` - Request job assignment (being implemented)

### Authentication
- All endpoints require `X-API-Key` header
- Default test API key: configurable via command line

## Testing Strategy

### Test Categories
1. **API Endpoint Tests (1-50)**: HTTP request/response validation
2. **State Management & Logic (51-100)**: Job state transitions, retry logic, persistence
3. **Core C++ & Miscellaneous (101-150)**: Thread safety, edge cases, refactoring support

### ApiTest Fixture (test_common.h)
- Provides `client` (httplib::Client) for HTTP requests
- Provides `api_key` for authentication
- Provides `clear_db()` for test isolation
- Sets up test server on random port for parallel execution

### Test Implementation Pattern
```cpp
TEST_F(ApiTest, TestName) {
    // 1. Setup test data (create jobs, engines as needed)
    nlohmann::json payload = {{"field", "value"}};
    httplib::Headers headers = {{"X-API-Key", api_key}};
    
    // 2. Make HTTP request
    auto res = client->Post("/endpoint", headers, payload.dump(), "application/json");
    
    // 3. Verify response
    ASSERT_EQ(res->status, 200);
    // Additional assertions...
}
```

## Build System

### Dependencies
- **httplib** - HTTP server/client library
- **nlohmann/json** - JSON parsing
- **Google Test** - Testing framework

### Build Commands
```bash
cd dispatch_server_cpp
mkdir -p build && cd build
cmake ..
make
./tests/dispatch_server_tests  # Run tests
```

## Data Structures

### Job Object
```json
{
  "job_id": "1704067200123456_0",
  "source_url": "http://example.com/video.mp4", 
  "target_codec": "h264",
  "job_size": 100.5,
  "status": "pending|assigned|completed|failed_permanently",
  "assigned_engine": "engine-123",
  "output_url": "http://example.com/video_out.mp4",
  "retries": 0,
  "max_retries": 3,
  "error_message": "Previous failure reason"
}
```

### Engine Object
```json
{
  "engine_id": "engine-123",
  "engine_type": "transcoder",
  "supported_codecs": ["h264", "vp9"],
  "status": "idle|busy",
  "storage_capacity_gb": 500.0,
  "streaming_support": true,
  "benchmark_time": 100.0
}
```

## State Management

### Job States
- `pending` → `assigned` → `completed` (success path)
- `assigned` → `pending` (retry after failure)
- `assigned` → `failed_permanently` (no retries left)

### Persistence
- Automatic state saving to `dispatch_server_state.json`
- Thread-safe operations with mutex protection
- Graceful handling of missing/corrupted state files

## Scheduling Logic

### Job Assignment Algorithm
- **Small jobs** (<50MB): Assign to slowest engine
- **Medium jobs** (50-100MB): Assign to fastest engine  
- **Large jobs** (≥100MB): Prefer streaming-capable engines
- Only idle engines considered for assignment
- Engine status changes to "busy" upon assignment

### Retry Logic
- `max_retries` defaults to 3
- Failed jobs increment retry counter
- Re-queue if retries < max_retries
- Permanent failure if retries ≥ max_retries

## Common Test Scenarios

### Basic Job Lifecycle
1. Submit job → status "pending"
2. Register engine → status "idle" 
3. Assign job → job "assigned", engine "busy"
4. Complete/fail job → job final state, engine "idle"

### Authentication Testing
- Valid API key → 200 OK
- Missing X-API-Key header → 401 + "Missing 'X-API-Key' header."
- Invalid API key → 401 Unauthorized

### Error Handling
- Missing required fields → 400 + specific error message
- Invalid JSON → 400 + "Invalid JSON: {details}"
- Non-existent resources → 404 + appropriate message

## Development Guidelines

### Git Workflow
- Implement one test at a time
- Verify test passes before proceeding
- Mark test completed in TESTING_TODO.md  
- Commit with descriptive message
- Push to remote after every commit

### Code Style
- Follow existing C++ conventions in codebase
- Use nlohmann::json for JSON handling
- Use httplib for HTTP operations
- Include proper error checking and validation

### Test Isolation
- Call `clear_db()` in test setup when needed
- Use unique identifiers to avoid conflicts
- Don't assume order of test execution

## Next Steps

### Immediate Priority
1. **Test 48**: Implement POST /assign_job/ endpoint test - job assignment when jobs pending and engines idle
2. **Test 49**: Test assignment when no jobs pending  
3. **Test 50**: Test assignment when jobs pending but no engines idle

### Future Priorities
- Complete State Management & Logic Tests (51-100)
- Implement Core C++ & Miscellaneous Tests (101-150)
- Consider implementation of actual assignment endpoint logic

## Troubleshooting

### Common Issues
- **Build failures**: Check CMakeLists.txt dependencies
- **Test failures**: Verify test server startup and API key configuration
- **Port conflicts**: Tests use random ports to avoid conflicts
- **State persistence**: Tests may need isolated state files

### Debugging Tips
- Use `std::cout` for debugging during development
- Check HTTP response bodies for detailed error messages
- Verify JSON payload structure matches expected format
- Ensure proper cleanup between tests

This document should be updated as new features are implemented and testing progresses.