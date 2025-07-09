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

## 1.1 Job Submission

**Endpoint:** `POST /jobs/`  
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
  "max_retries": 3
}
```

**Required Fields:**
- `source_url` (string): URL to the source video file
- `target_codec` (string): Target codec for transcoding

**Optional Fields:**
- `job_size` (number): Size of the job in MB (default: 0.0)
- `max_retries` (integer): Maximum retry attempts (default: 3)

**Success Response:** `200 OK`
```json
{
  "job_id": "1704067200123456_0",
  "source_url": "http://example.com/video.mp4",
  "target_codec": "h264",
  "job_size": 100.5,
  "status": "pending",
  "assigned_engine": null,
  "output_url": null,
  "retries": 0,
  "max_retries": 3
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

**Endpoint:** `GET /jobs/{job_id}`  
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

**Endpoint:** `GET /jobs/`  
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

# Part 2: Engine API (Dispatch Server ↔ Transcoding Engines/Compute Nodes)

This section documents the REST API endpoints used by transcoding engines (compute nodes) to interact with the dispatch server for engine registration, job assignment, and job completion reporting.

## 2.1 Engine Registration/Heartbeat

**Endpoint:** `POST /engines/heartbeat`  
**Purpose:** Register a new engine or update an existing engine's status

**Request Headers:**
```http
Content-Type: application/json
X-API-Key: {api_key}
```

**Request Body:**
```json
{
  "engine_id": "engine-123",
  "engine_type": "transcoder",
  "supported_codecs": ["h264", "vp9"],
  "status": "idle",
  "storage_capacity_gb": 500.0,
  "streaming_support": true,
  "benchmark_time": 100.0
}
```

**Required Fields:**
- `engine_id` (string): Unique identifier for the engine

**Optional Fields:**
- `engine_type` (string): Type of engine
- `supported_codecs` (array): List of supported codecs
- `status` (string): Current engine status ("idle", "busy")
- `storage_capacity_gb` (number): Available storage in GB (must be non-negative)
- `streaming_support` (boolean): Whether engine supports streaming
- `benchmark_time` (number): Benchmark performance metric (must be non-negative)

**Success Response:** `200 OK`
```
Heartbeat received from engine {engine_id}
```

**Error Responses:**
- `400 Bad Request: 'engine_id' is missing.`
- `400 Bad Request: 'engine_id' must be a string.`
- `400 Bad Request: 'storage_capacity_gb' must be a number.`
- `400 Bad Request: 'storage_capacity_gb' must be a non-negative number.`
- `400 Bad Request: 'streaming_support' must be a boolean.`
- `400 Invalid JSON: {error_details}`

## 2.2 List All Engines

**Endpoint:** `GET /engines/`  
**Purpose:** Retrieve a list of all registered engines

**Request Headers:**
```http
X-API-Key: {api_key}
```

**Success Response:** `200 OK`
```json
[
  {
    "engine_id": "engine-123",
    "engine_type": "transcoder",
    "supported_codecs": ["h264", "vp9"],
    "status": "idle",
    "storage_capacity_gb": 500.0,
    "streaming_support": true,
    "benchmark_time": 100.0
  }
]
```

**Empty Response:** `200 OK`
```json
[]
```

## 2.3 Job Completion

**Endpoint:** `POST /jobs/{job_id}/complete`  
**Purpose:** Mark a job as completed by a transcoding engine

**Request Headers:**
```http
Content-Type: application/json
X-API-Key: {api_key}
```

**Request Body:**
```json
{
  "output_url": "http://example.com/video_out.mp4"
}
```

**Required Fields:**
- `output_url` (string): URL to the completed transcoded file

**Success Response:** `200 OK`
```
Job {job_id} marked as completed
```

**Error Responses:**
- `404 Job not found` - Job ID does not exist
- `400 Bad Request: 'output_url' must be a string.`
- `400 Invalid JSON: {error_details}`

## 2.4 Job Failure

**Endpoint:** `POST /jobs/{job_id}/fail`  
**Purpose:** Mark a job as failed by a transcoding engine

**Request Headers:**
```http
Content-Type: application/json
X-API-Key: {api_key}
```

**Request Body:**
```json
{
  "error_message": "Transcoding failed due to codec incompatibility"
}
```

**Required Fields:**
- `error_message` (string): Description of the failure reason

**Success Response:** `200 OK`
```
Job {job_id} re-queued
```
or
```
Job {job_id} failed permanently
```

**Behavior:**
- If `retries < max_retries`: Job is re-queued with status "pending" and retries count incremented
- If `retries >= max_retries`: Job status becomes "failed_permanently"
- Assigned engine status returns to "idle" after job failure

**Error Responses:**
- `404 Job not found` - Job ID does not exist
- `400 Bad Request: 'error_message' is missing.` - When error_message field is missing
- `400 Bad Request: Job is already in a final state.` - When job is already completed or permanently failed
- `400 Invalid JSON: {error_details}` - When request body is malformed JSON

## 2.5 Engine Benchmark Results

**Endpoint:** `POST /engines/benchmark_result`  
**Purpose:** Submit benchmark performance data from a transcoding engine

**Request Headers:**
```http
Content-Type: application/json
X-API-Key: {api_key}
```

**Request Body:**
```json
{
  "engine_id": "engine-123",
  "benchmark_time": 150.5
}
```

**Required Fields:**
- `engine_id` (string): Unique identifier for the engine
- `benchmark_time` (number): Performance metric in seconds (must be non-negative)

**Success Response:** `200 OK`
```
Benchmark result received from engine {engine_id}
```

**Error Responses:**
- `404 Engine not found` - Engine ID does not exist
- `400 Bad Request: 'benchmark_time' must be a number.`
- `400 Bad Request: 'benchmark_time' must be a non-negative number.`
- `400 Invalid JSON: {error_details}`

## 2.6 Job Assignment (Pending Documentation)

**Endpoint:** `POST /assign_job/`  
**Purpose:** Request assignment of a pending job to an available engine

*This endpoint is currently being implemented and documented as part of tests 48-50.*

## 2.7 Storage Pool Configuration

**Endpoint:** `GET /storage_pools/`  
**Purpose:** Retrieve storage pool configuration (placeholder endpoint)

**Request Headers:**
```http
X-API-Key: {api_key}
```

**Success Response:** `200 OK`
```
Storage pool configuration to be implemented.
```

---

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
  "job_id": "1704067200123456_0",
  "source_url": "http://example.com/video.mp4", 
  "target_codec": "h264",
  "job_size": 100.5,
  "status": "assigned",
  "assigned_engine": "engine-123",
  "output_url": "http://example.com/video_out.mp4",
  "retries": 1,
  "max_retries": 3,
  "error_message": "Previous transcoding attempt failed"
}
```

**Field Descriptions:**
- `job_id` (string): Unique job identifier
- `source_url` (string): URL to source video file
- `target_codec` (string): Target codec for transcoding
- `job_size` (number): Size of job in MB
- `status` (string): Current job state
- `assigned_engine` (string|null): ID of assigned engine (null if unassigned)
- `output_url` (string|null): URL to transcoded output (null until completed)
- `retries` (integer): Number of retry attempts made
- `max_retries` (integer): Maximum allowed retry attempts
- `error_message` (string): Last error message (present only after failure)

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

*This document is updated as new protocol features are implemented and tested.*
