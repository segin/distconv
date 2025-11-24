# Submission Protocol

## Overview

This protocol defines the REST API used by **Submission Clients** to interact with the **Dispatch Server** for job management.

## Authentication

All endpoints require an API key via the `X-API-Key` header. If the server is configured without an API key, all requests are allowed.

## Endpoints

- **POST /api/v1/jobs** – Submit a new job. JSON body includes `source_url`, `target_codec`, optional `job_size`, `max_retries`, `priority`.
- **GET /api/v1/jobs/{job_id}** – Retrieve job status.
- **GET /api/v1/jobs** – List all jobs.
- **DELETE /api/v1/jobs/{job_id}** – Cancel a pending or assigned job.
- **POST /api/v1/jobs/{job_id}/retry** – Retry a failed job.
- **GET /api/v1/version** – Server version information.
- **GET /api/v1/status** – System health and statistics.

## Responses

- Successful responses use `200 OK` (or `201 Created` for job submission) with JSON payloads.
- Errors are returned as JSON with fields `error`, `error_type`, and appropriate HTTP status codes (`400`, `401`, `404`, `500`).
