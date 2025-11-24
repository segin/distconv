# Distributed Transcoding Protocol Documentation

This document defines the communication protocols between the three main components of the distributed transcoding system:

1. **Submission Clients** ↔ **Dispatch Server** (Client API)
2. **Dispatch Server** ↔ **Transcoding Engines/Compute Nodes** (Engine API)

## Overview

The system uses HTTP REST API for all communication, with JSON payloads for structured data exchange. All endpoints require API key authentication via the `X-API-Key` header.

## Authentication

All API endpoints require authentication using an API key passed in the `X-API-Key` header:

```http
X-API-Key: your_api_key_here
```

**Error Responses:**
- `401 Unauthorized: Missing 'X-API-Key' header.` - When header is missing
- `401 Unauthorized` - When API key is incorrect

# Part 1: Client API (Submission Client ↔ Dispatch Server)

This section documents the REST API endpoints used by submission clients to interact with the dispatch server for job management.

## API v1 Features

The dispatch server API v1 provides:
- Enhanced validation and error handling
- Structured JSON error responses with error codes
- Content-Type validation (`application/json` required)
- UUID-based job identifiers (36-character format)
- Job cancellation, retry, and priority management
- Engine deregistration capabilities
- Version and status endpoints

## 1.1 Job Submission

**Endpoint:** `POST /api/v1/jobs`  
**Purpose:** Submit a new transcoding job

**Request Headers:**
```http
Content-Type: application/json
X-API-Key: {api_key}
```

**Request Body:**
```json
{
  "source_url": "http://example.com/video.mp4",
  "target_codec": "h264",
  "job_size": 100.5,
  "max_retries": 3,
  "priority": 1
}
```

**Required Fields:**
- `source_url` (string): URL to the source video file
- `target_codec` (string): Target codec for transcoding

**Optional Fields:**
- `job_size` (number): Size of the job in MB (default: 0.0)
- `max_retries` (integer): Maximum retry attempts (default: 3)
- `priority` (integer): Job priority level (default: 0)
  - `0`: Normal priority
  - `1`: High priority  
  - `2`: Urgent priority

**Success Response:** `200 OK`
```json
{
  "job_id": "550e8400-e29b-41d4-a716-446655440000",
  "source_url": "http://example.com/video.mp4",
  "target_codec": "h264",
  "job_size": 100.5,
  "status": "pending",
  "assigned_engine": null,
  "output_url": null,
  "retries": 0,
  "max_retries": 3,
  "priority": 1,
  "created_at": 1704067200123,
  "updated_at": 1704067200123
}
```

**Error Responses:**
- `400 Bad Request: 'source_url' is missing or not a string.`
- `400 Bad Request: 'target_codec' is missing or not a string.`
- `400 Bad Request: 'job_size' must be a number.`
- `400 Bad Request: 'job_size' must be a non-negative number.`
- `400 Bad Request: 'max_retries' must be an integer.`
- `400 Bad Request: 'max_retries' must be a non-negative integer.`
- `400 Invalid JSON: {error_details}`

## 1.2 Job Status Retrieval

**Endpoint:** `GET /api/v1/jobs/{job_id}`  
**Purpose:** Get the current status of a specific job

**Request Headers:**
```http
X-API-Key: {api_key}
```

**Success Response:** `200 OK`
```json
{
  "job_id": "1704067200123456_0",
  "source_url": "http://example.com/video.mp4",
  "target_codec": "h264",
  "job_size": 100.5,
  "status": "completed",
  "assigned_engine": "engine-123",
  "output_url": "http://example.com/video_out.mp4",
  "retries": 0,
  "max_retries": 3
}
```

**Error Responses:**
- `404 Job not found` - Job ID does not exist

## 1.3 List All Jobs

**Endpoint:** `GET /api/v1/jobs`  
**Purpose:** Retrieve a list of all jobs

**Request Headers:**
```http
X-API-Key: {api_key}
```

**Success Response:** `200 OK`
```json
[
  {
    "job_id": "1704067200123456_0",
    "source_url": "http://example.com/video.mp4",
    "target_codec": "h264",
    "job_size": 100.5,
    "status": "completed",
    "assigned_engine": "engine-123",
    "output_url": "http://example.com/video_out.mp4",
    "retries": 0,
    "max_retries": 3
  }
]
```

**Empty Response:** `200 OK`
```json
[]
```

# Part 3: Shared Data Structures and Concepts

## Job States

Jobs progress through the following states:

1. **pending** - Job submitted, waiting for assignment
2. **assigned** - Job assigned to an engine, work in progress  
3. **completed** - Job successfully completed
4. **failed_permanently** - Job failed and exhausted all retries

**State Transitions:**
- `pending` → `assigned` (via job assignment)
- `assigned` → `completed` (via job completion)
- `assigned` → `pending` (via job failure with retries remaining)
- `assigned` → `failed_permanently` (via job failure with no retries remaining)

**Final States:** `completed` and `failed_permanently` are terminal states and cannot be changed.

## Engine States

Engines can be in the following states:

1. **idle** - Available for job assignment
2. **busy** - Currently processing a job

**State Transitions:**
- `idle` → `busy` (when assigned a job)
- `busy` → `idle` (when job completes or fails)

## Job Data Structure

Complete job object contains the following fields:

```json
{
  "job_id": "550e8400-e29b-41d4-a716-446655440000",
  "source_url": "http://example.com/video.mp4", 
  "target_codec": "h264",
  "job_size": 100.5,
  "status": "assigned",
  "assigned_engine": "engine-123",
  "output_url": "http://example.com/video_out.mp4",
  "retries": 1,
  "max_retries": 3,
  "priority": 1,
  "created_at": 1704067200123,
  "updated_at": 1704067250789,
  "error_message": "Previous transcoding attempt failed"
}
```

**Field Descriptions:**
- `job_id` (string): Unique UUID job identifier
- `source_url` (string): URL to source video file
- `target_codec` (string): Target codec for transcoding
- `job_size` (number): Size of job in MB
- `status` (string): Current job state
- `assigned_engine` (string|null): ID of assigned engine (null if unassigned)
- `output_url` (string|null): URL to transcoded output (null until completed)
- `retries` (integer): Number of retry attempts made
- `max_retries` (integer): Maximum allowed retry attempts
- `priority` (integer): Job priority level (0=normal, 1=high, 2=urgent)
- `created_at` (integer): Job creation timestamp (milliseconds since epoch)
- `updated_at` (integer): Last update timestamp (milliseconds since epoch)
- `error_message` (string): Error message from last failure (only present when status is failed)

## Recent Protocol Improvements

The following enhancements have been made to improve reliability, performance, and usability:

### 1. UUID-based Job Identifiers
- **Previous:** Time-based job IDs (`1704067200123456_0`) that could collide under high load
- **Current:** UUID-based job IDs (`550e8400-e29b-41d4-a716-446655440000`) that are globally unique
- **Benefit:** Eliminates potential job ID collisions in high-throughput scenarios

### 2. Job Priority System
- **New Field:** `priority` (integer) with values 0=normal, 1=high, 2=urgent
- **Benefit:** Allows prioritization of critical transcoding jobs
- **Usage:** Higher priority jobs are processed before lower priority ones

### 3. Enhanced Timestamps
- **New Fields:** `created_at` and `updated_at` (milliseconds since epoch)
- **Benefit:** Enables better tracking of job lifecycle and performance analytics
- **Usage:** Helps identify stale jobs and measure processing times

### 4. Improved Thread Safety
- **Enhancement:** All database operations now use proper mutex locking
- **Benefit:** Eliminates race conditions in concurrent job submissions
- **Implementation:** Dedicated mutexes for jobs and engines data structures

### 5. Asynchronous State Persistence
- **Enhancement:** State saving is now performed asynchronously to avoid blocking request handlers
- **Benefit:** Improved response times for API calls
- **Implementation:** Atomic file operations with temporary files and renames

### 6. Background Processing
- **New Feature:** Automatic cleanup of stale engines and job timeout handling
- **Benefit:** System self-maintenance without manual intervention
- **Implementation:** Background worker thread running every 30 seconds

### 7. Job Timeout Handling
- **Enhancement:** Jobs assigned to engines are automatically failed after 30 minutes of inactivity
- **Benefit:** Prevents jobs from getting stuck indefinitely
- **Implementation:** Background monitoring with automatic engine release

### 8. Structured Domain Objects
- **Enhancement:** Introduction of `Job` and `Engine` structs for type safety
- **Benefit:** Better code organization and reduced JSON manipulation errors
- **Implementation:** Proper serialization/deserialization methods

## Engine Data Structure

Complete engine object contains the following fields:

```json
{
  "engine_id": "engine-123",
  "engine_type": "transcoder",
  "supported_codecs": ["h264", "vp9", "av1"],
  "status": "idle",
  "storage_capacity_gb": 500.0,
  "streaming_support": true,
  "benchmark_time": 100.0
}
```

**Field Descriptions:**
- `engine_id` (string): Unique engine identifier
- `engine_type` (string): Type of transcoding engine
- `supported_codecs` (array): List of supported video codecs
- `status` (string): Current engine state ("idle" or "busy")
- `storage_capacity_gb` (number): Available storage capacity in GB
- `streaming_support` (boolean): Whether engine supports streaming mode
- `benchmark_time` (number): Performance benchmark metric in seconds

## Job Scheduling Logic

The dispatch server uses intelligent scheduling algorithms to assign jobs to engines:

### Job Size Categories
- **Small jobs** (`job_size < 50.0 MB`): Assigned to slowest available engine
- **Medium jobs** (`50.0 MB ≤ job_size < 100.0 MB`): Assigned to fastest available engine  
- **Large jobs** (`job_size ≥ 100.0 MB`): Preferred assignment to streaming-capable engines, fallback to fastest engine

### Engine Selection Criteria
1. **Engine availability**: Only "idle" engines are considered
2. **Benchmark data**: Engines must have `benchmark_time` data for prioritization
3. **Streaming support**: Large jobs prefer engines with `streaming_support: true`
4. **Performance ranking**: Engines sorted by `benchmark_time` (lower is faster)

### Assignment Behavior
- Only one job assigned per engine at a time
- Busy engines are not assigned additional jobs
- Engine status changes to "busy" upon job assignment
- Job status changes to "assigned" upon assignment
- `assigned_engine` field populated with engine ID upon assignment
- No assignment occurs if no idle engines available
- Engines without benchmark data are ignored during assignment
- Assignment process updates both job and engine records atomically

## Retry and Failure Handling

### Retry Logic
- `max_retries` defaults to 3 if not specified in job submission
- `max_retries` can be set to 0 for no retry attempts
- Each failure increments the `retries` counter
- Jobs are re-queued (status → "pending") if `retries < max_retries`
- Jobs become "failed_permanently" if `retries >= max_retries`

### Engine Status on Job Failure
- Assigned engine status returns to "idle" when job fails
- Engine becomes available for new job assignments immediately

## Job ID Format

Job IDs are generated using the format: `{microseconds}_{counter}`

Example: `1704067200123456_0`

- `1704067200123456` - Microseconds since epoch
- `0` - Atomic counter for uniqueness

## Error Handling

All endpoints return appropriate HTTP status codes:

- `200 OK` - Successful operation
- `204 No Content` - Successful operation with no response body
- `400 Bad Request` - Invalid request data or missing required fields
- `401 Unauthorized` - Missing or invalid API key
- `404 Not Found` - Requested resource does not exist
- `500 Internal Server Error` - Server-side error

## Content Types

All requests and responses use `application/json` content type for structured data, except for simple text responses which use `text/plain`.

## State Persistence

The dispatch server implements persistent state management:

### Storage Mechanism
- All job and engine data is persisted to JSON files
- Default storage file: `dispatch_server_state.json`
- State is automatically saved after any data modification
- State is loaded on server startup
- JSON files are well-formatted with indentation for readability
- Special characters and Unicode are properly escaped and preserved
- Atomic file operations prevent corruption (temporary file + rename)

### Auto-save Triggers
State is automatically saved when:
- New job is submitted (`SubmitJobTriggersSaveState`)
- Engine sends heartbeat (`HeartbeatTriggersSaveState`)
- Job is completed (`CompleteJobTriggersSaveState`)
- Job is failed (`FailJobTriggersSaveState`)
- Job is assigned to engine (`AssignJobTriggersSaveState`)

### State File Format
```json
{
  "jobs": {
    "job_id_1": { /* job object */ },
    "job_id_2": { /* job object */ }
  },
  "engines": {
    "engine_id_1": { /* engine object */ },
    "engine_id_2": { /* engine object */ }
  }
}
```

### Recovery Behavior
- Server gracefully handles missing state files (starts with empty state)
- Server gracefully handles corrupted JSON files (starts with empty state and logs error)
- Server gracefully handles partial state files (missing jobs or engines sections)
- Server gracefully handles empty JSON files (starts with empty state)
- Server gracefully handles malformed JSON syntax (parse errors logged, empty state used)
- Server gracefully handles files with only "jobs" key (engines initialized as empty)
- Server gracefully handles files with only "engines" key (jobs initialized as empty)
- State loading failures do not prevent server startup

## Thread Safety

The dispatch server implements thread-safe operations:

### Concurrency Control
- All shared state access is protected by mutex locks
- Multiple concurrent requests are safely handled
- Job and engine databases are thread-safe
- State persistence operations are atomic

### Concurrent Operations
- Multiple job submissions can occur simultaneously (`SubmitMultipleJobsConcurrently`)
- Multiple engine heartbeats can be processed concurrently (`SendMultipleHeartbeatsConcurrently`)
- Job assignment and completion can happen in parallel (`ConcurrentlyAssignJobsAndCompleteJobs`)
- Mixed operations (jobs + heartbeats) maintain consistency (`ConcurrentlySubmitJobsAndSendHeartbeats`)
- Database access is thread-safe with proper locking (`AccessJobsDbFromMultipleThreadsWithLocking`, `AccessEnginesDbFromMultipleThreadsWithLocking`)
- All operations maintain data consistency

---

# Part 4: Edge Cases and Robustness

## Server Robustness and Error Handling

Based on comprehensive testing, the dispatch server handles various edge cases gracefully:

### Input Validation Edge Cases

**Negative Values:**
- `job_size < 0`: Rejected with `400 Bad Request: 'job_size' must be a non-negative number.`
- `max_retries < 0`: Rejected with `400 Bad Request: 'max_retries' must be a non-negative integer.`
- `storage_capacity_gb < 0`: Rejected with `400 Bad Request: 'storage_capacity_gb' must be a non-negative number.`
- `benchmark_time < 0`: Rejected with `400 Bad Request: 'benchmark_time' must be a non-negative number.`

**Type Validation:**
- `streaming_support` must be boolean: Rejected with `400 Bad Request: 'streaming_support' must be a boolean.`
- Invalid JSON types for required fields trigger appropriate error messages

**Large Input Handling:**
- Extremely long `source_url` values (>10KB): Handled gracefully, either accepted or rejected with proper error response
- Extremely long `engine_id` values (>5KB): Handled gracefully, either accepted or rejected with proper error response
- Very large request bodies (>1MB): Handled gracefully, either processed or rejected with HTTP 413 or appropriate error

### High-Volume Operations

**Scale Testing Results:**
- Server handles 1000+ concurrent jobs without performance degradation
- Server handles 500+ concurrent engines without performance degradation
- Job assignment and completion work correctly under high load
- List operations (`GET /jobs/`, `GET /engines/`) remain responsive with large datasets

### Client Connection Issues

**Disconnection Handling:**
- Server gracefully handles client disconnections mid-request
- Server remains responsive after connection timeouts
- No server crashes or invalid states from network issues

### Job ID and Engine ID Edge Cases

**Numeric-Looking IDs:**
- Job IDs that look like numbers but are strings are handled correctly
- Non-existent job IDs return proper `404 Not Found` responses
- Very large numeric strings in job IDs are processed correctly

### Orphaned Operations

**Non-existent Resource Handling:**
- Attempting to complete jobs that were never assigned: Handled gracefully
- Attempting to fail jobs that were never assigned: Handled gracefully
- Sending heartbeats for engines with orphaned job references: Handled gracefully
- Benchmark results for non-existent engines: Returns `404 Engine not found`

### URL Path Handling

**Trailing Slash Behavior:**
- `GET /jobs/` and `POST /jobs/` work correctly with trailing slashes
- `GET /engines/` works correctly with trailing slashes
- Consistent behavior across all endpoints

### Content-Type Flexibility

**Header Handling:**
- Requests with non-standard `Content-Type` headers are handled gracefully
- Server either processes JSON regardless of Content-Type or returns appropriate errors
- Server doesn't crash from mismatched content types

## State Management Testing

### JSON Parsing and Serialization

**Valid Request Processing:**
- All endpoint JSON request formats are properly validated
- Complex nested JSON structures are handled correctly
- Unicode and special characters are preserved through save/load cycles

### State Persistence Robustness

**File System Operations:**
- Single job persistence: `LoadStateWithSingleJob`, `SaveStateWithSingleJob`
- Single engine persistence: `LoadStateWithSingleEngine`, `SaveStateWithSingleEngine`
- Empty state handling: `SaveStateWithZeroJobsAndZeroEngines`, `LoadStateFromZeroJobsAndZeroEnginesFile`
- Atomic file operations prevent corruption during save operations
- Formatted JSON output for human readability

**Recovery Scenarios:**
- Graceful handling of missing state files
- Graceful handling of corrupted JSON files
- Graceful handling of files with partial data (only jobs or only engines)
- Error logging without preventing server startup

### Unique ID Generation

**Job ID Uniqueness:**
- Job ID generation tested with 10,000 rapid submissions
- Zero collisions observed in tight-loop generation
- Format: `{microseconds}_{counter}` ensures uniqueness

## Testing Infrastructure

### Mocking and Test Isolation

**Available Test Utilities:**
- `SaveStateCanBeMocked`: Prevents actual file I/O during testing
- `LoadStateCanBeMocked`: Provides controlled state data for testing
- `JobsDbCanBeClearedAndPrePopulated`: Allows test data setup
- `EnginesDbCanBeClearedAndPrePopulated`: Allows test engine setup
- `PersistentStorageFileCanBePointedToTemporaryFile`: Prevents test interference

### Server Instance Control

**Test Server Management:**
- `run_dispatch_server` can be called without argc/argv arguments
- API keys can be set programmatically without command-line arguments
- `httplib::Server` instance accessible for advanced testing scenarios
- Random port assignment prevents test conflicts: `find_available_port()`
- Server can be started and stopped programmatically from tests

### Concurrent Testing

**Thread Safety Validation:**
- All database operations are thread-safe with proper mutex locking
- Concurrent job submissions, engine heartbeats, and assignments work correctly
- No race conditions observed in state transitions
- No data corruption under concurrent load

## Performance Characteristics

### Response Times

**Typical Response Times:**
- Job submission: < 5ms
- Job status retrieval: < 2ms  
- Job assignment: < 10ms
- Engine heartbeat: < 3ms
- List operations: < 5ms (for datasets up to 1000 items)

### Memory Usage

**Memory Characteristics:**
- Linear memory usage with number of jobs and engines
- No memory leaks observed during extended testing
- Efficient JSON serialization and deserialization

### State File Performance

**File I/O Performance:**
- Atomic save operations complete in < 10ms for typical datasets
- Well-formatted JSON files remain human-readable
- State loading is fast even for large datasets (tested up to 1000 jobs/500 engines)

---

*This document reflects the current state of the protocol as validated by a comprehensive 150-test suite covering all major functionality, edge cases, and robustness scenarios.*
