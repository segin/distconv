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

## 2.4 Job Failure (Pending Documentation)

**Endpoint:** `POST /jobs/{job_id}/fail`  
**Purpose:** Mark a job as failed by a transcoding engine

*This endpoint is currently being implemented and documented as part of tests 43-47.*

## 2.5 Job Assignment (Pending Documentation)

**Endpoint:** `POST /assign_job/`  
**Purpose:** Request assignment of a pending job to an available engine

*This endpoint is currently being implemented and documented as part of tests 48-50.*

---

# Part 3: Shared Data Structures and Concepts

## Job States

Jobs progress through the following states:

1. **pending** - Job submitted, waiting for assignment
2. **assigned** - Job assigned to an engine, work in progress
3. **completed** - Job successfully completed
4. **failed_permanently** - Job failed and exhausted all retries

## Engine States

Engines can be in the following states:

1. **idle** - Available for job assignment
2. **busy** - Currently processing a job

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

---

*This document is updated as new protocol features are implemented and tested.*
