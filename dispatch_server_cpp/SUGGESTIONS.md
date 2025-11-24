# Dispatch Server Improvement Checklist

This checklist contains 100 specific suggestions for improving the dispatch server component.

## Database & Thread Safety
- [x] Add Mutexes for Database Access: The global `jobs_db` and `engines_db` are read from and written to by multiple threads without any locking, which is a major race condition. Add `std::mutex` and `std::lock_guard` around all accesses to these objects.
- [x] Replace JSON file with SQLite: For better performance, transactional safety, and to avoid read/write race conditions, replace the `dispatch_server_state.json` file with an SQLite database. (Repositories implementation with SQLite already exists)
- [x] Don't block in request handlers: The `save_state()` function performs synchronous file I/O within the HTTP request handler, which will block the server's worker thread. Offload this to a separate thread.
- [x] The server state file is a single point of failure. Implement backups or use a more robust database.
- [x] `save_state()` is not transactional. If the server crashes mid-write, the JSON file will be corrupt. Write to a temp file and rename.

## Code Structure & Architecture
- [x] Refactor `main.cpp`: The `main.cpp` file is too large. Refactor it by creating classes for `JobManager`, `EngineManager`, and `ApiServer`. (DispatchServer class with proper separation exists)
- [x] Create a `Server` class: Encapsulate all server setup and endpoint registration logic within a `Server` class instead of doing it all in `main`.
- [x] Separate the storage logic into a `Storage` class. (Repository pattern implemented)
- [x] Create a `Job` struct/class to represent a job instead of manipulating raw `nlohmann::json` objects everywhere.
- [x] Create an `Engine` struct/class.
- [ ] Eliminate Global State: Refactor to remove global variables like `API_KEY`, `jobs_db`, and `engines_db`. Pass them as dependencies (e.g., in a context/application object).

## Job Management & Scheduling
- [x] Improve Job Assignment Logic: The current logic just picks the first pending job and assigns it to a complexly-chosen engine. Improve this by considering job priority, job size, and engine load.
- [x] Job re-submission logic is flawed: A failed job is just marked "pending". It should have a separate "failed_retry" state and a backoff delay before being re-queued.
- [x] Handle `job_id` collisions: Using a millisecond timestamp as a job ID is not guaranteed to be unique under high load. Use a UUID or a database sequence.
- [x] The server assumes all jobs are the same. Different transcoding tasks have different resource requirements. The scheduler should be aware of this.
- [x] The `/assign_job` endpoint is inefficient: It iterates all jobs every time. Use a dedicated queue or a database query to find the next pending job.
- [x] The server should have a concept of job priority, settable at submission time.
- [x] Add endpoint to manually re-submit a failed job.
- [x] Add endpoint to cancel a pending or assigned job.
- [x] The server should handle the case where an engine takes a job but then crashes before completing it. A timeout mechanism is needed on the server side for assigned jobs.

## API & Endpoints
- [x] Validate `Content-Type` header: The `/jobs/` endpoint should check that the `Content-Type` is `application/json`.
- [x] Add a `/version` endpoint to return the server's build version and commit hash.
- [x] Add a `/status` endpoint for a simple health check.
- [x] Endpoint for engine de-registration: Add an endpoint for an engine to signal it's shutting down, so the server can remove it from the pool.
- [x] Add endpoint to update a job's parameters after submission.
- [x] Add endpoint to get a list of jobs assigned to a specific engine.
- [x] The `/jobs/{id}/complete` and `/fail` endpoints should probably be a single `/jobs/{id}/status` endpoint using `PUT` or `PATCH`.
- [x] Add mechanism for engine to report progress on a job. Add a `/jobs/{id}/progress` endpoint.
- [x] The storage pool endpoint is a placeholder. It should be implemented to allow adding, removing, and querying storage locations.

## Error Handling & Validation
- [ ] Better JSON error handling: The `try-catch` blocks are good, but provide more specific error messages to the client about what was wrong with their JSON payload.
- [ ] The server doesn't validate the `output_url` provided by the engine. It should be checked for validity.
- [ ] Handle exceptions from `std::to_string` or `stod`.
- [ ] The `max_retries` value should be validated to be a non-negative integer.
- [ ] When loading state, if the JSON is corrupt, the server currently just logs an error and starts with an empty state. It should perhaps exit or use a backup file.

## Engine Management
- [ ] Heartbeat should update timestamp: The heartbeat should update a `last_seen` timestamp for the engine.
- [ ] Stale engine cleanup: Implement a background task to remove engines that haven't sent a heartbeat in a configurable amount of time.
- [ ] Engine selection in `/assign_job` can be optimized: The sorting of engines on every call is inefficient for a large number of engines. Maintain sorted lists or use a more efficient data structure.
- [ ] Sort engine list: The `/engines/` endpoint should provide a sorted list, e.g., by hostname or last heartbeat.
- [ ] The server should track `ffmpeg` versions of each engine and potentially refuse to assign jobs that require features not present in an engine's `ffmpeg` version.
- [ ] Provide a way for an admin to put an engine in "maintenance mode", preventing it from being assigned new jobs.
- [ ] The `/engines/` list should show the current job being processed by each busy engine.
- [ ] There's no way to get historical performance data for an engine.
- [ ] The server should track not just the assigned engine, but a history of all engines a job was assigned to.

## Performance & Scalability
- [ ] Don't use `std::endl`: Replace `std::endl` with `\n` to avoid unnecessary buffer flushes, especially in logging.
- [ ] Sort job list: The `/jobs/` endpoint should default to sorting by submission time.
- [ ] Add filtering to list endpoints: e.g., `/jobs?status=pending`.
- [ ] Add a proper thread pool for background tasks like cleaning up stale engines, instead of creating detached threads.
- [ ] `main.cpp:115` - looping through all jobs to find a pending one is O(N). Use a separate queue of pending job IDs.
- [ ] `main.cpp:125` - sorting engines on every request is O(M log M). This is very inefficient.
- [ ] The server should support HTTP/2 for better performance. `httplib` can be configured for this.
- [ ] Use `std::string_view` in function parameters to avoid unnecessary string copies.

## Configuration & Deployment
- [ ] Make port configurable: The port `8080` is hardcoded. Make it a command-line argument.
- [ ] Remove hardcoded paths for SSL certs: Make `cert_path` and `key_path` configurable.
- [ ] The API key should be read from an environment variable if not provided on the command line.
- [ ] The `PERSISTENT_STORAGE_FILE` path should be configurable.
- [ ] Make the server listen on `127.0.0.1` by default for better security, and require an explicit `--host 0.0.0.0` to listen publicly.
- [ ] The logic for `large_job_threshold` is arbitrary. This should be configurable.
- [ ] Make `max_retries` a server-side configuration as well, to prevent clients from setting unreasonable values.

## Security & Authentication
- [ ] The API key check is repeated in every handler. Use a middleware or a pre-routing handler to perform the check once.
- [ ] The `X-API-Key` header is non-standard. The standard is `Authorization: Bearer <key>`.
- [ ] The server should handle `SIGPIPE` gracefully when a client disconnects.
- [ ] Add support for CORS headers to allow a web-based submission client.

## Code Quality & Standards
- [ ] Use `std::optional`: For fields like `assigned_engine` and `output_url`, use `std::optional` instead of `nullptr` in the C++ representation before serializing to JSON.
- [ ] Add `const` to request handlers: The lambda for request handlers should take a `const httplib::Request&`.
- [ ] Use structured binding: In loops over maps like `jobs_db.items()`, use structured bindings: `for (const auto& [key, val] : ...)` (Already done, good).
- [ ] Don't use raw loops over `argv`. Use a proper command-line parsing library like `cxxopts` or `CLI11`.
- [ ] Use `nlohmann::json::value()` method to provide default values safely. (Already used, good).
- [ ] Don't rely on `req.matches[1]`. This is brittle. Give names to capture groups in the regex or use a routing library that provides named parameters.

## Logging & Monitoring
- [ ] Use a logger library: Replace all `std::cout` and `std::cerr` with a proper logging library like `spdlog`.
- [ ] Logging request details: Log the remote IP and method for each incoming request.
- [ ] Log the reason for job assignment decisions.
- [ ] The server should store and expose its own logs via an API endpoint for easier debugging.
- [ ] The job failure logic increments `retries` but doesn't log the previous error messages, losing history.

## Build System & Dependencies
- [ ] Improve CMake `find_package`: Instead of hardcoded paths for `httplib` and `json`, use `find_package` or `FetchContent`.
- [ ] Link OpenSSL properly in CMake: Use `find_package(OpenSSL REQUIRED)` and link with `${OPENSSL_LIBRARIES}`. (This is already done, good job!).
- [ ] Add CPack to CMake to create distributable packages (.deb, .rpm).
- [ ] Consider using a C++ web framework (e.g., Crow, Oat++) instead of raw `httplib` for more features like middleware and better routing.

## Advanced Features
- [ ] Graceful Shutdown: Implement a signal handler (for `SIGINT`, `SIGTERM`) to call `svr.stop()` and `save_state()` for a clean shutdown.
- [ ] The server's main loop doesn't handle any signals. It can only be killed with `SIGKILL`.
- [ ] Implement "soft delete" for jobs instead of permanent deletion, to maintain history.
- [ ] The server should have a concept of users or tenants. All jobs are in a single global pool.
- [ ] Provide a way for an admin to manually assign a specific job to a specific engine.
- [ ] The server should track which storage pools are available and their capacity.
- [ ] The benchmark result is just a single number. It should be a more detailed report (e.g., time per codec, per resolution).
- [ ] The `benchmark_time` should be associated with a specific task (e.g., `h264_1080p_encode_seconds`) not just a generic value.

## REST API Design
- [ ] The `/assign_job/` is a POST but doesn't create a resource. It should probably be a `GET` on a resource like `/queue/next_job`. This is a REST design issue.
- [ ] Unnecessary JSON parsing: The `/assign_job/` endpoint parses a JSON body just to get the `engine_id`. This could be a query parameter.
- [ ] The server should return a `Retry-After` header when rate limiting a client.
- [ ] The server should support chunked transfer encoding for large request/response bodies.
- [ ] API documentation (Swagger/OpenAPI) should be auto-generated from the code or a spec file.
- [ ] The API should support partial responses (e.g., `fields=job_id,status`) to reduce bandwidth.

## Data Management
- [ ] The concept of "job size" is vague. It should be clearly defined (e.g., in MB, or duration in seconds).
- [ ] The `std::chrono` timestamp is good for uniqueness but might not be the best job ID for users. Consider a shorter, more human-readable ID (e.g., `job-f9b4c1`).
- [ ] `failed_permanently` is a terminal state. There should be a way to re-queue such a job manually via the API.
- [ ] In the job assignment, if no streaming-capable engine is found for a large job, what happens? The logic falls back to the fastest, but this might fail. The scheduler should handle this more gracefully.