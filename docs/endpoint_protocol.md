# Endpoint Protocol

## Overview

This protocol defines the REST API used by **Transcoding Engines/Compute Nodes** to interact with the **Dispatch Server** for job execution, status updates, and engine management.

## Authentication

All endpoints require an API key via the `X-API-Key` header. If the server is configured without an API key, all requests are allowed.

## Endpoints

- **POST /api/v1/engines/heartbeat** – Register a new engine or update an existing engine’s status. JSON body includes `engine_id`, `engine_type`, `supported_codecs`, `status`, `storage_capacity_gb`, `streaming_support`, `benchmark_time`.
- **POST /api/v1/jobs/{job_id}/complete** – Mark a job as completed. JSON body includes `output_url`.
- **POST /api/v1/jobs/{job_id}/fail** – Report a job failure. JSON body includes `error_message`.
- **POST /api/v1/engines/benchmark_result** – Submit benchmark performance data for an engine. JSON body includes `engine_id` and `benchmark_time`.

## Responses

- Successful operations return `200 OK` (or `201 Created` where appropriate) with plain‑text or JSON payloads.
- Errors are returned as JSON with fields `error`, `error_type`, and appropriate HTTP status codes (`400`, `401`, `404`, `500`).

## Error Handling

All error responses follow the same structure as the Submission Protocol, providing clear `error_type` (e.g., `validation_error`, `server_error`) and a human‑readable `error` message.

## State Updates

- **Job Completion:** Updates `jobs_db` with `status: "completed"` and stores `output_url`. Frees the assigned engine (`status: "idle"`).
- **Job Failure:** Increments `retries`. If `retries < max_retries`, the job is re‑queued (`status: "pending"`); otherwise, it becomes `failed_permanently`. The engine status is set to `idle`.
- **Engine Heartbeat:** Creates or updates engine entries in `engines_db`, setting `status` and other metadata.
