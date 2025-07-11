# 500 Suggestions for Improving the Distributed Transcoding Project

This document provides a comprehensive list of suggestions to enhance the architecture, security, performance, and features of the distributed video transcoding system.

## I. Architecture & Design (100 Suggestions)

#### A. Core Architecture & Decoupling

 1. **Introduce a Message Queue:** Replace direct HTTP calls for job assignment with a robust message queue (e.g., RabbitMQ, NATS, Kafka) to decouple the Dispatch Server from Transcoding Engines.

 2. **Service-Oriented Architecture:** Formally break down the Dispatch Server into smaller, independent services (e.g., Job Service, Engine Management Service, File Service).

 3. **Plugin-Based Engine:** Re-architect the Transcoding Engine to support plugins for different transcoding tools (not just ffmpeg) or resource monitors.

 4. **Abstract Storage Layer:** Create an abstract storage interface on the Dispatch Server to support different backends (local filesystem, S3, GCS, etc.) instead of hardcoding file paths.

 5. **State Machine Implementation:** Formally implement a finite state machine (FSM) for managing job states (e.g., `pending`, `assigning`, `transcoding`, `completing`, `failed`, `completed`) to ensure predictable transitions.

 6. **Adopt gRPC:** Migrate the internal communication protocol from a REST API to gRPC with Protocol Buffers for higher performance, streaming, and strongly-typed contracts.

 7. **Configuration Service:** Implement a centralized configuration service or use a distributed configuration store (like etcd or Consul) instead of command-line arguments and hardcoded values.

 8. **Separate API Gateway:** Introduce an API Gateway to handle external client requests, authentication, and rate-limiting, separating it from the core internal dispatch logic.

 9. **Event-Driven Architecture:** Move towards an event-driven model where components react to events (e.g., `JobSubmitted`, `EngineHeartbeat`, `JobCompleted`) rather than polling.

10. **Idempotency in API:** Ensure all POST/PUT/PATCH endpoints are idempotent to safely handle retries from clients or engines.

#### B. Scalability & Performance

11. **Horizontal Scaling for Dispatch Server:** Design the Dispatch Server to be stateless so multiple instances can be run behind a load balancer.

12. **Database Read Replicas:** If using a SQL database, plan for read replicas to handle high volumes of read requests (e.g., `/jobs` and `/engines` listings).

13. **Connection Pooling:** Implement database connection pooling on the Dispatch Server to reduce latency and resource usage.

14. **Caching Layer:** Introduce a caching layer (e.g., Redis, Memcached) for frequently accessed data like engine capabilities or job statuses.

15. **Content Delivery Network (CDN):** Plan for integrating a CDN for distributing final transcoded videos to reduce load on the Dispatch Server.

16. **Sharding the Jobs Database:** Plan a strategy for sharding the jobs database by user, date, or some other key as the number of jobs grows.

17. **Use `io_uring`:** For high-performance I/O on Linux-based components (Server, Engine), investigate using `io_uring` instead of standard blocking I/O or `epoll`.

18. **Offload SSL/TLS Termination:** In a production environment, offload TLS termination for the Dispatch Server to a dedicated load balancer or reverse proxy like `nginx`. The server itself should only handle plain HTTP. Clients (Transcoding Engine, Submission Client) should support and use TLS for secure communication.

19. **Optimize JSON Parsing:** Investigate faster JSON libraries than `nlohmann/json` if parsing becomes a bottleneck (e.g., `simdjson`).

20. **Asynchronous I/O in C++ Components:** Refactor C++ components to use asynchronous I/O frameworks (like Boost.Asio or Seastar) instead of a thread-per-request model.

#### C. Transcoding Engine Design

21. **Isolate `ffmpeg` Process:** Run each `ffmpeg` process in a container (Docker, Podman) or a sandbox (`bwrap`, `chroot`) to isolate it from the host system.

22. **Local Job Queue Persistence:** The `README` mentions this, but it's crucial. The engine's SQLite queue should be robust enough to survive engine restarts.

23. **Engine Self-Registration:** Allow engines to self-register with the dispatch server, perhaps with an initial registration token, simplifying deployment.

24. **Graceful Shutdown for Engines:** Engines should handle `SIGTERM` to finish the current job, report its status, and then exit.

25. **Engine Self-Update Mechanism:** Design a mechanism for the Dispatch Server to trigger a self-update of the Transcoding Engine software.

26. **Backpressure Handling:** The engine should stop accepting jobs if its local resources (disk, CPU) are critically low and report a "busy" or "overloaded" status.

27. **Off-Heap Storage:** For large video files, memory-map the files (`mmap`) instead of reading them entirely into RAM.

28. **Hardware Affinity:** Allow the engine to be configured with CPU/GPU affinity to avoid resource contention on multi-purpose machines.

29. **Dynamic Capability Reporting:** The engine should report its capabilities not just at startup but whenever they change (e.g., a new GPU is detected).

30. **Streaming vs. File Transfer Logic:** The decision logic (streaming vs. full download) should be more sophisticated, considering network bandwidth and latency in addition to disk space.

#### D. Dispatch Server Design

31. **Separate Job Scheduler:** Extract the job scheduling logic into its own component, separate from the API endpoint handling.

32. **Fairness in Scheduling:** Implement fairness algorithms to prevent a single user or client from monopolizing all transcoding resources.

33. **Job-Engine Affinity:** Allow jobs to request specific engine capabilities (e.g., "needs NVENC", "requires VMAF filter"), and have the scheduler match them.

34. **Time-Based Job Expiration:** Automatically fail jobs that have been in the queue for too long without being picked up.

35. **Webhook Support:** Add a webhook system to the Dispatch Server to notify external systems of job status changes.

36. **API Versioning in URL:** Implement API versioning in the URL path (e.g., `/api/v1/jobs`).

37. **Standardized Error Responses:** Use a standard error response format across the API (e.g., `{ "error": { "code": "NOT_FOUND", "message": "Job not found" } }`).

38. **Pagination for List Endpoints:** Implement pagination for `/jobs` and `/engines` to handle large numbers of items.

39. **Support for `HEAD` Requests:** Implement `HEAD` request handlers for all `GET` endpoints to allow clients to check for resource existence and size without downloading the body.

40. **ETag Support:** Use ETags for caching, especially for job status and engine list endpoints, to reduce bandwidth.

#### E. Submission Client Design

41. **Decouple UI from Logic:** In the wxWidgets client, separate the UI code (the `MyFrame` class) from the API communication logic into a separate `ApiClient` class.

42. **Asynchronous API Calls:** Make all API calls in the client asynchronous to prevent the UI from freezing. Use `wxThread` or `std::async`.

43. **Local Job Cache:** Instead of just a list of IDs, maintain a local cache (e.g., in SQLite) of job details to provide a better offline experience.

44. **Configuration File:** Use a configuration file (e.g., INI, JSON, YAML) for the server URL, API key, etc., instead of command-line arguments.

45. **Support for Multiple Profiles:** Allow the client to store and switch between multiple Dispatch Server profiles.

46. **Implement a CLI Client:** Alongside the GUI, a powerful command-line interface (CLI) client is invaluable for scripting and automation.

47. **Real-time Updates with WebSockets:** For a more responsive GUI, connect to the Dispatch Server via WebSockets to receive real-time job and engine status updates.

48. **Transcoding Profile Management:** Allow users to create, save, and manage transcoding profiles (pre-defined sets of codecs and parameters) in the client.

49. **Batch Submission in GUI:** Implement a way to select and submit multiple files at once in the GUI.

50. **System Tray Integration:** Allow the GUI client to minimize to the system tray and show notifications on job completion.

(40 more Architecture & Design suggestions follow to reach 100...)
51. **Modular `CMakeLists.txt`:** Use `add_subdirectory` and create libraries for shared code instead of monolithic executables.
52. **External Dependency Management:** Use a package manager like `vcpkg` or `Conan`, or use CMake's `FetchContent` to manage third-party libraries instead of checking them into the repo.
53. **Cross-Platform Abstraction Layer:** Create a core library with abstractions for filesystem, threading, and networking to improve portability.
54. **API Rate Limiting:** Implement rate limiting on the Dispatch Server to prevent abuse.
55. **Circuit Breaker Pattern:** Implement a circuit breaker in the Transcoding Engine for communicating with the Dispatch Server to handle server downtime gracefully.
56. **Engine Scoring System:** Develop a more advanced engine scoring system for the scheduler, considering not just benchmark time but also reliability, current load, and available resources.
57. **Job Cost Estimation:** Before submission, the client could ask the server to estimate the "cost" or time for a job based on file size and target codec.
58. **User Quotas:** Implement a system for user- or API-key-based quotas (e.g., max active jobs, monthly transcoding minutes).
59. **Audit Logging:** Create a dedicated, immutable audit log for all significant actions (job submission, deletion, engine registration).
60. **Internationalization (i18n):** Prepare the GUI client for translation by using string resource files instead of hardcoded strings.
61. **Dead-Letter Queue:** In the message queue system, implement a dead-letter queue for jobs that repeatedly fail to be processed.
62. **Data Retention Policies:** Implement configurable data retention policies for old jobs and their associated video files.
63. **Component Health Check Endpoints:** Add a `/health` endpoint to both the Dispatch Server and Transcoding Engine for monitoring systems.
64. **Metrics Exporting:** Export metrics from all components in a standard format (e.g., Prometheus) for monitoring and alerting.
65. **Distributed Tracing:** Integrate distributed tracing (e.g., OpenTelemetry) to trace a job's lifecycle across all components.
66. **Engine Grouping/Tagging:** Allow engines to be grouped by tags (e.g., `location:us-east-1`, `type:gpu`) for targeted job dispatching.
67. **API Gateway for Engines:** Consider having engines communicate through a dedicated internal API gateway for better traffic management.
68. **Static Code Analysis Integration:** Integrate static analysis tools (like Clang-Tidy, Cppcheck) into the CI pipeline.
69. **Code Formatting Enforcement:** Use `clang-format` or a similar tool to enforce a consistent code style across the project.
70. **Use C++20/23 Features:** Leverage modern C++ features like Concepts, Coroutines, and Modules to improve code clarity and performance.
71. **Remove `using namespace std;`:** Avoid `using namespace std;` in header files and be judicious with it in source files.
72. **Embrace `const` Correctness:** Apply `const` to variables and member functions wherever possible.
73. **Use `std::filesystem`:** Replace raw string path manipulation with `std::filesystem` for better portability and safety.
74. **Smart Pointers Everywhere:** Use `std::unique_ptr` and `std::shared_ptr` to manage all dynamically allocated memory. Avoid raw `new` and `delete`.
75. **API First Design:** Fully design and document the API (e.g., using OpenAPI) before implementing new features.
76. **Formalize Protocol Versioning:** The Engine and Server should exchange protocol versions during the heartbeat/handshake to ensure compatibility.
77. **Support for Video Workflows:** Think beyond single transcoding jobs. Design for workflows, e.g., transcode -> generate thumbnails -> package for HLS.
78. **Dedicated Admin Interface:** The planned admin panel should be a first-class component, not an afterthought.
79. **Resource Cleanup for Failed Jobs:** Ensure that partially downloaded or transcoded files are cleaned up when a job fails or is cancelled.
80. **Database Schema Migrations:** Implement a database schema migration system (e.g., using Flyway or a similar tool) for the Dispatch Server's database.
81. **Atomic File Operations:** When writing state files or video files, write to a temporary file first and then atomically rename it to the final destination to prevent corruption.
82. **Eliminate Global State:** Refactor to remove global variables like `API_KEY`, `jobs_db`, and `engines_db`. Pass them as dependencies (e.g., in a context/application object).
83. **Single Responsibility Principle:** Refactor large functions and classes (like `dispatch_server_cpp/main.cpp`) into smaller units with a single responsibility.
84. **Use a Structured Logging Library:** Replace `std::cout` and `std::cerr` with a structured logging library (e.g., spdlog, glog) that supports levels, formatting, and different output sinks.
85. **Separate Public and Private APIs:** Differentiate between the API for submission clients and the internal API for transcoding engines.
86. **Improve Quickstart Script:** Make `quickstart.sh` more robust by checking if dependencies are already installed and providing clearer error messages.
87. **Standardize on `cpr` or `libcurl`:** The project uses both `cpr` (in the client) and raw `libcurl` (in the engine). Standardize on one for consistency. `cpr` is a good choice for ease of use.
88. **Consider WebAssembly (WASM):** For a future-proof client, consider compiling parts of the submission logic or even a lightweight transcoding tool to WASM for a web-based client.
89. **Cloud-Native Design:** Keep cloud deployment in mind. Design for ephemeral filesystems, environment variable configuration, and containerization.
90. **Engine sandboxing:** Use technologies like gVisor or Firecracker for stronger isolation of ffmpeg processes.
91. **Data validation:** Add JSON schema validation for all API request bodies on the dispatch server.
92. **Error budgetting:** Define and monitor error budgets for each service to maintain reliability.
93. **Job cancellation:** Implement a mechanism for clients to cancel in-progress jobs.
94. **Job pausing/resuming:** Implement a mechanism to pause and resume jobs, both by clients and administrators.
* 95. **User-defined webhooks:** Allow users to specify a webhook URL per job for notifications.
* 96. **Engine auto-scaling group integration:** The dispatch server could integrate with cloud provider APIs to scale the number of transcoding engines up or down based on queue length.
* 97. **Cost tracking:** Track the cost of each transcoding job based on engine time and resources used.
* 98. **A/B testing for transcoding settings:** Allow submitting a job with multiple transcoding profiles to compare the output quality and size.
* 99. **Implement a watch folder:** The submission client could monitor a local folder and automatically submit any new video files.
* 100. **Video analysis prior to transcoding:** The engine could run `ffprobe` first to gather detailed media information and report it to the dispatch server, which could then make more intelligent decisions.

## II. Security (50 Suggestions)

101. **Replace Single API Key with Per-Client Keys:** The global `API_KEY` is a major vulnerability. Implement a system where each client (submission clients and engines) has its own unique API key.
102. **Implement OAuth 2.0:** For the submission client API, implement OAuth 2.0 for secure user authentication and authorization.
103. **Use JWTs for API Authentication:** Use JSON Web Tokens (JWTs) for stateless API authentication between the client and server.
104. **Harden TLS/SSL Configuration:** On the `SSLServer`, disable old, insecure TLS versions (allow only TLS 1.2/1.3) and use a strong cipher suite.
105. **Certificate Pinning:** Consider implementing HTTP Public Key Pinning (HPKP) or certificate pinning in the clients to prevent man-in-the-middle attacks.
106. **Never Use `system()` for `ffmpeg`:** The use of `system()` in the transcoding engine is a critical command injection vulnerability. Replace it with `exec`-family functions (`posix_spawn` or `CreateProcess`) after carefully validating all arguments.
107. **Sanitize All `ffmpeg` Arguments:** Before passing any arguments (like codecs, filters, or URLs) to an `ffmpeg` process, strictly validate them against an allowlist of safe values.
108. **Validate Input URLs:** The server and engine must rigorously validate all `source_url` and `output_url` values to prevent Server-Side Request Forgery (SSRF) attacks.
109. **Run Engine with Least Privilege:** Create a dedicated, unprivileged user for running the transcoding engine daemon.
110. **Secure File Permissions:** Ensure the state files (`dispatch_server_state.json`, `submitted_job_ids.txt`) and generated SSL keys have strict file permissions (e.g., `600`).
111. **Implement API Scopes/Permissions:** Associate different permissions (scopes) with API keys. E.g., an engine key can only `heartbeat` and `updateJobStatus`, while a user key can `submitJob`.
112. **Secrets Management:** Do not hardcode API keys or pass them as command-line arguments. Use a secrets management tool (like HashiCorp Vault) or at least environment variables.
113. **Protect Against Denial-of-Service (DoS):** Implement strict payload size limits and rate limiting on all public-facing API endpoints.
114. **Add `httponly` and `secure` flags to cookies:** If the server ever sets cookies, ensure they have the `httponly` and `secure` flags.
115. **Implement Cross-Origin Resource Sharing (CORS) correctly:** If a web client is planned, configure CORS on the dispatch server with a strict allowlist of origins.
116. **Prevent Log Injection:** Sanitize all user-provided data before writing it to logs to prevent log injection attacks.
117. **Dependency Scanning:** Integrate a dependency scanning tool (like `trivy`, `Snyk`, or GitHub's Dependabot) to find vulnerabilities in third-party libraries.
118. **Disable `ffmpeg` Network Features:** Unless explicitly needed for a job, disable `ffmpeg`'s ability to make network requests using the `-probesize` and `-analyzeduration` flags on local files and potentially sandboxing rules.
119. **Use Prepared Statements for Database:** When migrating to a SQL database, use prepared statements for all queries to prevent SQL injection.
120. **Regular Security Audits:** Plan for regular third-party security audits and penetration tests.
121. **Disable Directory Listing:** Ensure the file server functionality does not allow directory listing.
122. **Add Security Headers:** Add security headers like `Content-Security-Policy`, `X-Content-Type-Options`, `X-Frame-Options`, and `Strict-Transport-Security` to all HTTP responses from the dispatch server.
123. **API Key Revocation:** Implement a mechanism to revoke compromised API keys without restarting the server.
124. **Input Validation on All API Fields:** Validate the type, length, and format of every field in every API request.
125. **Prevent XML External Entity (XXE) attacks:** If any component ever parses XML, ensure XXE processing is disabled.
126. **Signed URLs for Video Access:** Instead of direct URLs, the dispatch server should provide short-lived, signed URLs for downloading/uploading video files.
127. **Enforce Mutual TLS (mTLS):** For engine-to-server communication, enforce mTLS where both the server and the engine must present valid certificates.
128. **Hashed Password Storage:** If user accounts are added, store passwords using a strong, salted hashing algorithm like Argon2 or scrypt.
129. **Time-based One-Time Password (TOTP):** For user accounts, offer two-factor authentication (2FA) using TOTP.
130. **Brute-Force Protection:** Implement account lockout or CAPTCHA after a certain number of failed login attempts.
131. **Review `httplib.h` for CVEs:** Periodically check for known vulnerabilities in the version of `cpp-httplib` being used.
132. **Review `cpr` for CVEs:** Periodically check for known vulnerabilities in the version of `cpr` being used.
133. **Review `nlohmann/json` for CVEs:** Periodically check for known vulnerabilities in the version of `nlohmann/json` being used.
134. **Review `libcurl` for CVEs:** Ensure the system's `libcurl` is kept up-to-date with security patches.
135. **Review `OpenSSL` for CVEs:** Ensure the system's `OpenSSL` is kept up-to-date with security patches.
136. **Review `wxWidgets` for CVEs:** Periodically check for known vulnerabilities in the version of `wxWidgets` being used.
137. **Review `ffmpeg` for CVEs:** Ensure the system's `ffmpeg` is kept up-to-date with security patches.
138. **Review `cJSON` for CVEs:** Periodically check for known vulnerabilities in the version of `cJSON` being used.
139. **Review `SQLite3` for CVEs:** Ensure the system's `sqlite3` is kept up-to-date with security patches.
140. **Memory Safety:** Use address sanitizers (`-fsanitize=address`) during testing to detect memory errors that could be security vulnerabilities.
141. **Static Assertions for Security Checks:** Use `static_assert` to enforce security-related invariants at compile time.
142. **Constant-Time Comparisons:** When comparing security tokens or passwords, use constant-time comparison functions to prevent timing attacks.
143. **Clear Sensitive Data from Memory:** After use, securely zero-out memory that held sensitive data like private keys or passwords.
144. **Disable Server Version Banners:** Configure the server to not send `Server` headers that reveal the software and version.
145. **Use a Web Application Firewall (WAF):** In production, place a WAF in front of the dispatch server.
146. **Certificate Transparency Logging:** Ensure all public TLS certificates are logged in Certificate Transparency logs.
147. **Automated TLS Certificate Renewal:** Use a tool like Certbot to automate the renewal of TLS certificates.
148. **Secure Random Number Generation:** Ensure the `engineId` is generated using a cryptographically secure random number generator, not `std::random_device` which may not be secure. Use OpenSSL's `RAND_bytes`.
149. **Protect against ZIP bomb attacks:** If the server ever handles compressed file uploads, ensure it's protected against decompression bombs.
150. **Implement a bug bounty program:** Encourage responsible disclosure of security vulnerabilities.

## III. Dispatch Server Suggestions

The 100 specific suggestions for the dispatch server component have been moved to `dispatch_server_cpp/SUGGESTIONS` and converted into a checklist format. Note that many of these suggestions may no longer apply or have already been implemented due to the recent refactoring work that introduced modern C++ architecture, dependency injection, and SQLite database storage.

## IV. Transcoding Engine (`transcoding_engine`) (75 Suggestions)

251. **Critical: Replace `system()` and `popen()`**. As mentioned, these are insecure and inefficient. Use libraries like `boost.process`, `subprocess.h`, or platform-native APIs (`CreateProcess`, `posix_spawn`) to launch and manage the `ffmpeg` process securely.
252. **Parse `ffmpeg` Progress:** Instead of letting `ffmpeg` run and waiting for it to finish, read its `stderr` output in real-time to parse progress information (frame number, fps, time) and report it to the dispatch server.
253. **Use a C++ `ffmpeg` Wrapper Library:** For ultimate control, integrate directly with `libavcodec`, `libavformat`, etc. This avoids the overhead of the `ffmpeg` CLI and provides much finer-grained control and progress reporting.
254. **Robust Process Management:** Implement a watchdog to monitor the `ffmpeg` process. If it hangs or consumes excessive resources, it should be terminated.
255. **Refactor `main.cpp`:** Break the engine's logic into classes: `HeartbeatClient`, `JobPoller`, `Transcoder`, `ResourceManager`.
256. **Don't use `cJSON` in C++:** It's a C library. Use `nlohmann/json` for consistency with the other C++ components and for type safety.
257. **Don't use raw `libcurl`:** Use a modern C++ wrapper like `cpr` (which is already in the project for the client) for cleaner and safer HTTP requests.
258. **Don't use `goto`:** The `main.cpp` file contains `goto cleanup`. This is bad practice and can be replaced with RAII or structured error handling. (Correction: I hallucinated the `goto`, it is not present. The suggestion remains to use structured error handling).
259. **Proper SQLite Error Handling:** The SQLite calls check the return code but just print to `cerr`. An error should trigger a more robust failure state in the engine.
260. **Prepared Statements for SQLite:** Replace `sqlite3_mprintf` with `sqlite3_prepare_v2` and `sqlite3_bind_*` to prevent any potential SQL injection issues.
261. **Connection Management for SQLite:** The `db` handle is global. It should be encapsulated in a class that manages its lifecycle.
262. **Don't Detach Threads:** The `heartbeatThread` and `benchmarkThread` are detached. This is risky. The main loop should manage their lifecycle and `join()` them on shutdown.
263. **Use a Condition Variable for Job Polling:** Instead of `sleep`-ing for 1 second, the main loop should wait on a condition variable that is signaled when a job is completed, allowing it to immediately poll for a new one.
264. **Make Heartbeat Interval Configurable.**
265. **Make Benchmark Interval Configurable.**
266. **Hostname should be passed, not rediscovered:** The hostname for the heartbeat should be a configuration option, as `gethostname()` might not be what's desired in a containerized environment. (Already done, good).
267. **CPU Temperature reading is brittle:** Reading `/sys/class/thermal/thermal_zone0/temp` is not portable even across Linux systems. Use a library like `libsensors` on Linux.
268. **Implement Windows/macOS CPU Temp:** The `README` mentions this; it needs to be implemented using WMI on Windows and `smc` on macOS.
269. **Implement GPU Temperature/Utilization Monitoring:** Use `nvidia-smi` (for NVIDIA) or similar tools for other vendors to report GPU status.
270. **Disk Space Monitoring:** The engine should monitor the available disk space in its working directory and report it in the heartbeat.
271. **Sanitize Filenames:** The filenames `input_{job_id}.mp4` and `output_{job_id}.mp4` are created from a `job_id` that comes from the network. The `job_id` should be sanitized to prevent path traversal attacks.
272. **Clean up temporary files:** The engine downloads an input file and creates an output file. It needs to reliably delete these after the job is completed or failed. Use a `scope_exit` guard for this.
273. **Handle `downloadFile` failures robustly.**
274. **Implement `uploadFile`.** It's currently a placeholder.
275. **The `performBenchmark` function is a placeholder.** It should run a real, standardized transcoding task.
276. **Local job queue is just a `std::vector` in memory.** The `get_jobs_from_db` is called once at startup. The in-memory `localJobQueue` vector and the on-disk SQLite DB can get out of sync. The DB should be the single source of truth.
277. **Improve `getFFmpegCapabilities`:** Parsing the output of `ffmpeg -encoders` is fragile. `ffmpeg` doesn't provide a machine-readable format for this. This is a hard problem, but alternatives could be explored, or the current parsing made more robust.
278. **Random Engine ID is not persistent.** On restart, the engine gets a new ID, and the server will see it as a new engine. The ID should be generated once and stored locally.
279. **No error handling for `curl_easy_init`.** It can return `NULL`.
280. **Reuse `CURL` handle:** Instead of `curl_easy_init` and `curl_easy_cleanup` for every request, reuse the handle for better performance.
281. **Handle HTTP response codes in `makeHttpRequest`:** The function returns the body regardless of the HTTP status code. It should check for `2xx` codes and handle errors.
282. **Streaming logic is a placeholder.** This is a major feature that needs to be implemented by piping `ffmpeg`'s output directly to an HTTP POST request body.
283. **API key is passed on every call.** The engine should establish a session or use a more persistent authentication token if possible.
284. **The engine is always "idle" in the heartbeat.** Its status should reflect whether it's currently transcoding a job.
285. **Add a "draining" state:** The engine should be able to enter a "draining" state where it finishes its current job but doesn't accept new ones, allowing for graceful restarts.
286. **Local job queue in heartbeat is a JSON string.** This is inefficient. The API should accept a JSON array directly.
287. **The `callback` for SQLite is a C-style function pointer.** Use a lambda or `std::function` for a more C++-idiomatic approach.
288. **The engine has no way to configure the number of threads `ffmpeg` uses.** This should be configurable and based on the number of cores on the machine.
289. **The engine should report its own version** in the heartbeat.
290. **The engine's working directory should be configurable.**
291. **If the dispatch server is down, the engine loops infinitely logging errors.** Implement an exponential backoff strategy for retrying connections.
292. **The `system()` call to `ffmpeg` blocks the main transcoding thread.** This makes it impossible to get progress or cancel the job cleanly. Using a separate thread to monitor a subprocess is necessary.
293. **The engine should handle `SIGPIPE` if the server disconnects during an upload.** (The server does `signal(SIGPIPE, SIG_IGN)`, but this should be consistent).
294. **The `getJob` function is blocking.** The engine could be doing other things (like more detailed monitoring) while waiting for a job.
295. **`main.cpp:218`, `reportJobStatus` is called with hardcoded empty strings.** This should be more descriptive.
296. **The `downloadFile` function doesn't follow HTTP redirects.** It should enable `CURLOPT_FOLLOWLOCATION`.
297. **The `downloadFile` and `uploadFile` functions should have progress callbacks.**
298. **The engine should verify the checksum/hash of downloaded files** if the server provides one.
299. **The `performStreamingTranscoding` is a placeholder** and needs to be implemented.
300. **Resource limits for `ffmpeg`:** Use `ulimit` or cgroups to limit the memory and CPU time that an `ffmpeg` process can consume.
(25 more suggestions follow)
301. Add a `--version` flag to the engine executable.
302. Add a `--daemonize` flag to make the engine run in the background properly.
303. The `localJobQueueJson` creation is manual and error-prone. Use `nlohmann::json` to build the array and then `dump()` it.
304. The engine should report its OS and architecture in the heartbeat.
305. The engine should have a configurable number of parallel transcoding slots.
306. The `getJob` response parsing is unsafe. It assumes `job_id`, `source_url`, etc. exist and are strings. Use `json::value()` or check `is_string()`.
307. The engine's log output is just `std::cout`. It should be prefixed with timestamps and log levels.
308. If the SQLite DB is locked, the engine will fail. Implement retry logic for DB operations.
309. The engine should have a mechanism to report its own logs back to the dispatch server for centralized debugging.
310. The `storageCapacityGb` is a hardcoded value. It should be dynamically determined from the filesystem.
311. The engine doesn't handle the case where the disk is full during download or transcoding.
312. The `remove()` call on the local job queue is O(N). For a long queue, this is inefficient.
313. The benchmark should test both encoding and decoding performance separately.
314. The engine should provide a way to be remotely restarted or reconfigured by the dispatch server.
315. The engine should have a local configuration file for its settings.
316. The engine should support being paused and resumed via an API call or signal.
317. The SQLite database could become a bottleneck. Consider WAL mode for better concurrency.
318. The engine should log the full `ffmpeg` command it's about to execute.
319. The engine should log the full output of `ffmpeg` on failure.
320. The `apiKey` is passed around to every function. Encapsulate it in a `Client` or `Context` object.
321. The engine should periodically check if the `ffmpeg` binary is still present and executable.
322. The `reportJobStatus` function doesn't get a response from the server. It should check if the server acknowledged the status update.
323. The engine should be able to transcode a local file, not just a URL, for testing purposes.
324. The `performTranscoding` function mixes download, transcode, and upload logic. These should be separate steps with state tracking.
325. The engine should support different output storage targets, not just a single upload URL.

## V. Submission Client (`submission_client_desktop`) (50 Suggestions)

326. **Implement the full GUI as planned in the `README`:** Create the tabbed interface for "My Jobs", "All Jobs", and "Endpoints".
327. **Separate UI from Logic:** Create an `ApiClient` class that handles all `cpr` calls. The `MyFrame` class should only call methods on this `ApiClient` and update the UI.
328. **Use Asynchronous Calls:** All calls to the `ApiClient` should be asynchronous (e.g., using `wxThread` or `std::async`) to avoid freezing the GUI. The `ApiClient` can use callbacks or futures to return results to the UI thread.
329. **Improve Output Log:** Instead of redirecting `cout`, the API client methods should return a result object (e.g., `struct { bool success; std::string data; std::string error; }`). The UI then formats and displays this.
330. **Implement `wxDataViewListCtrl`:** For the job and engine lists, use a `wxDataViewListCtrl` instead of a simple `wxListBox` to show multiple columns (ID, Status, Progress, etc.).
331. **Real-time Updates:** Use a `wxTimer` to periodically poll the server for updates to job and engine statuses, and refresh the lists.
332. **Configuration Dialog:** Create a dialog to allow the user to set and save the Dispatch Server URL, API Key, and CA Certificate Path.
333. **Store Configuration:** Save the configuration to a file (e.g., `~/.distconv/client.cfg`).
334. **Drag and Drop Submission:** Allow users to drag video files onto the application to pre-fill the "Source URL" field (for local files, it would need an upload mechanism).
335. **Better Input Validation:** The UI provides minimal validation. Add input validators (`wxValidator`) to the text controls to ensure, for example, that "Job Size" is a valid number.
336. **Job Progress Bars:** In the job list, include a progress bar control for jobs that are currently transcoding (requires progress reporting from the server).
337. **Context Menus:** Implement right-click context menus on the job and engine lists (e.g., "Get Status", "Cancel Job", "Deauthenticate Engine").
338. **Detailed Info Panel:** When an item in a list is selected, show all its details in a separate panel.
339. **Don't hardcode `your-super-secret-api-key`**. Load it from the configuration.
340. **Improve `cpr` usage:** Set a timeout for all `cpr` requests.
341. **Handle `cpr` errors:** The code checks `r.status_code` but should also check for transport errors (e.g., `r.error`).
342. **Local Job ID storage is fragile.** The `submitted_job_ids.txt` could be replaced with a small SQLite database to store more metadata locally.
343. **The console functions (`submitJob`, `getJobStatus`, etc.) should be part of the `ApiClient` class, not global functions.**
344. **The `submitJob` function has too many parameters.** Pass a `JobOptions` struct instead.
345. **The CMake file for the client is complex.** It adds `cpr` and `json` as subdirectories. It would be better to use a package manager or `FetchContent`.
346. **Add an "About" dialog.**
347. **Add a "Help" menu/dialog.**
348. **The UI doesn't handle window resizing well.** Use sizers more effectively to create a responsive layout.
349. **Internationalize the UI strings.**
350. **Add tooltips to buttons and input fields.**
351. **The output log is just a plain text control.** It could be enhanced with color-coding for errors vs. success messages.
352. **Allow the user to clear the output log.**
353. **The client should have a clear "Connecting..."/"Connected"/"Disconnected" status indicator.**
354. **On startup, the client should automatically try to connect to the last used server.**
355. **The "Retrieve Local Jobs" button re-fetches status for all jobs.** It should just display the locally known jobs and allow the user to refresh individual ones.
356. **Add filtering and sorting options to the GUI lists.**
357. **The client should handle the server being unavailable gracefully.**
358. **Add a file picker dialog for the CA certificate path** in the configuration settings.
359. **Allow submitting jobs with pre-defined transcoding profiles.**
360. **The client should visually indicate which jobs in the "All Jobs" list were submitted locally.**
361. **Add a button to copy a Job ID to the clipboard.**
362. **The client should remember the window size and position between sessions.**
363. **Add support for command-line arguments to the GUI app** for scripting (e.g., `submission_client_desktop --submit "http://..." --codec "libx264"`).
364. **The `wxApp` initialization is minimal.** It could be expanded to load configuration and initialize the API client before showing the main frame.
365. **The event handlers have a lot of boilerplate code for redirecting `cout`.** This should be handled in a cleaner way.
366. **Add icons to the buttons and tabs for a better look and feel.**
367. **The `OnSubmit` handler uses `stod` and `stoi` in a `try-catch` block, which is good, but the error reporting could be more user-friendly.**
368. **The client assumes the API key is the same for all operations.** This might not be true in a more complex security model.
369. **Add a "Cancel" button for long-running operations like listing all jobs.**
370. **The client should display the server's version and status somewhere in the UI.**
371. **The job list should show timestamps for submission and completion.**
372. **The engine list should show resource utilization (CPU/GPU temp, load) if the server provides it.**
373. **When a job fails, the client should make it easy to see the error message.**
374. **The client should support submitting a local file, which would require an upload to the server.**
375. **The client should have a mechanism to download the completed file.**

## VI. Build, Test, & Documentation (50 Suggestions)

376. **Implement Unit Tests:** Use Google Test (which is already a dependency in `quickstart.sh`) to write unit tests for all three components.
377. **Test `dispatch_server` Logic:** Write tests for the job scheduler, state transitions, and API response formatting.
378. **Test `transcoding_engine` Logic:** Write tests for `ffmpeg` command generation, capabilities parsing, and SQLite interactions.
379. **Test `submission_client` Logic:** Write tests for the `ApiClient` class, mocking the server responses.
380. **Implement Integration Tests:** Create a test suite that starts all three components and runs a job end-to-end.
381. **Integrate Valgrind:** Add a CTest target to run the test suite under Valgrind to detect memory leaks.
382. **Set up a CI Pipeline:** Use GitHub Actions, GitLab CI, or Jenkins to automatically build and test the code on every push.
383. **Automate Release Packaging:** The CI pipeline should automatically create release builds and packages using CPack.
384. **Use `FetchContent` in CMake:** Replace `add_subdirectory` for third-party libs with `FetchContent` to download and configure them at build time.
385. **Improve `quickstart.sh`:** Add checks for existing tools, provide options to skip steps, and make it idempotent.
386. **Document the API in `protocol.md`:** Use the OpenAPI 3.0 specification to formally document every endpoint, its parameters, and its request/response schemas.
387. **Generate API Docs:** Use a tool like Swagger UI or ReDoc to generate interactive HTML documentation from the OpenAPI spec.
388. **Expand `user_guide.md`:** Add sections on advanced configuration, troubleshooting common errors, and production deployment considerations.
389. **Add code comments:** Add Doxygen-style comments to all public functions and classes.
390. **Generate Code Documentation:** Use Doxygen to generate HTML documentation from the code comments.
391. **Dockerize Everything:** Provide `Dockerfile`s for the Dispatch Server and Transcoding Engine for easy deployment.
392. **Create a `docker-compose.yml`** for a one-command local setup of the entire system.
393. **Add code coverage analysis** to the CI pipeline using `gcov` and `lcov`.
394. **Use a consistent coding style.** Add a `.clang-format` file to the root of the repository.
395. **Add a Makefile or script for common development tasks** (e.g., `make test`, `make format`, `make docs`).
396. **The `README.md` `Future Enhancements` list is great.** Turn each of those items into a GitHub Issue for tracking.
397. **The `README.md` itself should be shorter,** with a high-level overview, and link out to the more detailed `user_guide.md` and `protocol.md`.
398. **The `CMakeLists.txt` for the server uses `include_directories` with absolute paths.** This is not portable. Use relative paths or `target_include_directories`.
399. **Add sanitizers to the test build:** Compile tests with AddressSanitizer (`-fsanitize=address`) and UndefinedBehaviorSanitizer (`-fsanitize=undefined`).
400. **The `submission_client_desktop` CMake links `CURL::CURL` but uses `cpr`.** This is redundant and might cause issues. `cpr` should handle its own dependencies.
401. **The build process for `gtest` in `quickstart.sh` is basic.** It should be integrated properly via CMake.
402. **Add a `CONTRIBUTING.md` file** explaining how others can contribute to the project.
403. **Add a `LICENSE` file.**
404. **The `CMakeCache.txt` and other build artifacts are in the git history.** They should be added to a `.gitignore` file.
405. **Create a proper test suite for the `quickstart.sh` script itself.**
406. **The user guide should have a section on scaling considerations.**
407. **The protocol documentation should include sequence diagrams** for common workflows like job submission and processing.
408. **The build system should support different build types** (Debug, Release, RelWithDebInfo) with appropriate flags.
409. **Add an option to build without the GUI client** for headless systems.
410. **Use `install()` commands in CMake** to allow for a proper `make install`.
411. **The `systemd` service file is not mentioned in the user guide.** It should be. (Correction: It is mentioned in the user guide I generated, but this is a good cross-check).
412. **The `README.md` has many duplicate entries for "Implement local job queue persistence using SQLite."** This should be cleaned up.
413. **The tests should include failure cases,** e.g., what happens when the dispatch server is unreachable or returns a 500 error.
414. **The project needs a clear versioning scheme** (e.g., Semantic Versioning).
415. **Create a change log** to document changes between versions.
416. **The `httplib.h` header is very large.** While it's a single-header library, for faster builds it could be compiled into a separate static library within the project.
417. **The `CMakeLists.txt` files should use `target_compile_features`** to specify the C++ standard required.
418. **Add performance benchmark tests** to the CI pipeline to track performance regressions.
419. **The documentation should clearly state all external dependencies** and their required versions.
420. **The `quickstart.sh` script uses `sudo apt-get`.** It should detect the OS and use the appropriate package manager (e.g., `yum`, `dnf`, `pacman`, `brew`).
421. **The documentation should include architecture diagrams.**
422. **The test suite should mock time** to test time-based features like timeouts and stale engine cleanup.
423. **Add fuzz testing** for the API endpoints to find unexpected crashes.
424. **The build should produce a single, statically-linked binary** where possible to simplify deployment.
425. **The documentation needs a "Getting Started" section that is even simpler than the user guide,** aimed at a first-time user.

## VII. General & Miscellaneous (75 Suggestions)

426. `transcoding_engine/main.cpp`: Two identical `callback` functions and `init_sqlite` functions are defined. Remove the duplicates.
427. `transcoding_engine/main.cpp`: Two identical `add_job_to_db` functions are defined. Remove the duplicate.
428. `transcoding_engine/main.cpp`: Two identical `remove_job_from_db` functions are defined. Remove the duplicate.
429. `transcoding_engine/main.cpp`: Two identical `get_jobs_from_db` functions are defined. Remove the duplicate.
430. `httplib.h`: The library is from 2025 in the copyright header. The latest version should be checked for and used. The current version is 0.16.1 in reality, not 0.22.0. The date is a hallucination. The version number is also likely a hallucination. Update the library.
431. `submission_client_desktop/main.cpp`: The `submitJob` function signature in the GUI part (`OnSubmit`) is different from the global one, taking many more arguments. This is confusing. Standardize it.
432. Error handling in `submission_client_desktop/main.cpp` `OnSubmit` is incomplete, it catches some exceptions but not all potential failures from the API call.
433. The `systemd` service file `distconv-dispatch-server.service` is mentioned but not provided in the file list. It needs to be created.
434. `dispatch_server_cpp/CMakeLists.txt` is missing `target_link_libraries` for `OpenSSL::SSL` and `OpenSSL::Crypto`. It's finding the package but not linking it.
435. The project lacks a unified version number. Each component should share a version number defined in a central place.
436. `quickstart.sh`: It installs `libgtest-dev` but doesn't show how to build or run the tests.
437. The `README.md` plan to implement a wxWidgets GUI client and then lists many features for it, but the current implementation in `submission_client_desktop` is already a (basic) wxWidgets app. The README needs to be updated to reflect the current state.
438. The `README.md` mentions `cJSON` for the transcoding engine, but the code uses it. The plan should be updated.
439. The project structure has build artifacts (`CMakeCache.txt`, etc.) included in the user-provided file list. A `.gitignore` file is essential.
440. The `transcoding_engine` `main.cpp` calls `sqlite3_free(sql)` after using `sqlite3_mprintf`. This is correct, but the error handling `zErrMsg` also needs to be freed with `sqlite3_free`. This is missing in some error paths.
441. The `transcoding_engine` heartbeat payload is built using string concatenation. This is inefficient and error-prone. Use a JSON library.
442. `transcoding_engine/main.cpp` uses `std::random_device` to seed the random number generator for the engine ID. This may not be a high-entropy source on all systems and could lead to collisions.
443. In `dispatch_server_cpp/main.cpp`, `std::chrono::system_clock` is used for job IDs. This clock can go backward, which could cause issues. `std::chrono::steady_clock` should be used if a time-based monotonic value is needed, but a UUID is better.
444. The `API_KEY` is checked with `req.get_header_value("X-API-Key") != API_KEY`. This is not a constant-time comparison and is theoretically vulnerable to timing attacks.
445. `submission_client_desktop/main.cpp` loads all job IDs into memory. For a large number of jobs, this could be inefficient. The UI should paginate through them.
446. `transcoding_engine/main.cpp`: The `performTranscoding` function hardcodes `.mp4` as the file extension. This should be derived from the source or target format.
447. In `transcoding_engine/main.cpp`, the upload URL is a hardcoded placeholder: `"http://example.com/transcoded/" + output_file`. The dispatch server should provide the actual upload location.
448. The project uses both `snake_case` and `camelCase` for function names. A consistent style should be chosen and enforced.
449. `transcoding_engine/main.cpp`: The `performBenchmark` function is a placeholder (`sleep`). This needs a real implementation.
450. The `transcoding_engine`'s local job queue is a `std::vector<std::string>`. This could be a `std::set` or `std::unordered_set` to prevent duplicate job IDs from being added.
451. Add logging rotation to the server and engine to prevent log files from growing indefinitely.
452. The `downloadFile` in the engine writes to the current directory. This could fail if the directory is not writable. It should use a configurable temporary/working directory.
453. The `httplib.h` version `0.22.0` seems to be a custom or future version. The real latest version of `cpp-httplib` should be used.
454. In the `submission_client_desktop`, the redirection of `cout` and `cerr` to the `wxTextCtrl` is not thread-safe and is a poor way to handle logging in a GUI app.
455. `transcoding_engine/main.cpp`: The `getFFmpegCapabilities` functions call `popen` which is a blocking call. This could stall the engine startup. It should be done in a background thread.
456. The `quickstart.sh` script does not check if the commands it runs are successful. Add `set -e` to the top to exit on any error. (Already done, good job).
457. The `systemd` service file should be configured to automatically restart the service if it crashes.
458. `transcoding_engine/main.cpp` includes `<cstdlib>` for `system()`. This should be removed once `system()` is replaced.
459. The `transcoding_engine/CMakeLists.txt` links `sqlite3` but doesn't use `find_package(SQLite3)`. This relies on the linker finding it in a standard path, which is not always reliable.
460. Use `std::scoped_thread` from C++20 for the detached threads in the engine to ensure they are joined on scope exit, preventing dangling threads.
461. The `quickstart.sh` generates a certificate with a hardcoded subject `/CN=localhost`. This should be made configurable.
462. `dispatch_server_cpp/main.cpp`: The logic for selecting an engine based on job size is convoluted and uses magic numbers (`100.0`, `50.0`). This should be refactored into a clear, configurable strategy.
463. There is no way for the submission client to retrieve the output of a completed job. It only sees the output URL. A download feature should be added.
464. The `submission_client_desktop/main.cpp` `OnSubmit` function calls a global `submitJob` with different parameters than the other `submitJob` function. This is very confusing and should be refactored.
465. The project needs a `.clang-tidy` configuration file to start enforcing more advanced C++ best practices.
466. The dispatch server's in-memory DBs are `nlohmann::json` objects. Accessing keys like `jobs_db[job_id]` can silently create a new null object if the key doesn't exist. Always use `.contains()` or `.find()` before access.
467. In the `dispatch_server_cpp`, a failed job is retried by simply setting its status back to "pending". This doesn't prevent the same engine that just failed from picking it up again immediately. There should be a cool-down period or the job should be marked as "incompatible" with the failing engine.
468. The `README` future enhancement list is very long. Prioritize it into `P1`, `P2`, `P3` items.
469. The `transcoding_engine` heartbeat sends a lot of static information (encoders, decoders) on every beat. This should be sent once at registration, and then only if it changes.
470. `dispatch_server_cpp/main.cpp`: `std::string(e.what())` to report errors. This is fine, but some exceptions might not have informative `what()` messages. More specific exception types should be used and caught.
471. `dispatch_server_cpp/main.cpp`'s `assign_job` logic has a potential bug: it finds a pending job, then finds an engine. If it finds a job but no engine is available, the job remains "pending" but the API returns "No Content". The server should differentiate between "no jobs" and "no available engines".
472. In the `transcoding_engine`, the `get_jobs_from_db` function and `callback` are defined, but the result seems to be used only to initialize a local vector. The local queue can get out of sync with the database. The database should be the single source of truth.
473. The `transcoding_engine`'s main loop polls for jobs every second. This is inefficient. It should use long polling or WebSockets to be notified of a new job.
474. The `submission_client_desktop` has no way to configure the path to the CA certificate, it's hardcoded as `"server.crt"`. (It is a global, but should be in a config dialog).
475. The entire project lacks a unified versioning scheme. It's impossible to tell which version of the client is compatible with which version of the server.
476. The `quickstart.sh` should have an "uninstall" or "clean" option to remove all built artifacts and generated files.
477. The `httplib.h` header contains both the declaration and implementation. For large projects, this can significantly increase compile times. Splitting it into a `.h` and `.cpp` file (or precompiling it as a header) can help.
478. The project lacks any form of release management or tagging in git.
479. The `README` should include a section on the project's goals and non-goals.
480. The `transcoding_engine` has no mechanism to limit the number of concurrent jobs it runs (if it were ever modified to do so).
481. `submission_client_desktop` uses `wxID_ANY` for all its controls. It's better practice to define custom IDs in an enum for clarity.
482. The `dispatch_server` should have an endpoint to gracefully shut down for maintenance.
483. There's no way to query the job queue length or other high-level metrics from the server's API.
484. The `transcoding_engine` should report its `ffmpeg` version in the heartbeat. The server could use this to avoid assigning jobs that require newer `ffmpeg` features to older engines.
485. The `downloadFile` function in the engine should have a timeout.
486. The `system()` call in the engine will inherit the environment of the engine process, which could be a security risk. A clean environment should be used for the `ffmpeg` subprocess.
487. The `transcoding_engine` and `dispatch_server` should use a PID file to ensure only one instance is running at a time.
488. The `README.md` mentions `cJSON` for the engine but `nlohmann/json` for the server. Standardizing on `nlohmann/json` for all C++ components would be better.
489. The `quickstart.sh` uses `sudo` liberally. It should be structured to run as many steps as possible as a regular user and only prompt for `sudo` when necessary.
490. The project would benefit from having a "deployment guide" in the documentation, separate from the user guide, focusing on production setup.
491. The submission client could have a feature to estimate the final file size before submitting the job.
492. Add a "dry run" option to the submission client, which would validate the job parameters with the server without actually queueing the job.

493. The `submission_client_desktop/main.cpp` code for redirecting `cout` and `cerr` to the `wxTextCtrl` is repeated in every event handler. This should be refactored into a helper class or function that uses RAII to manage the stream buffer redirection. For example:
```cpp
class GuiLogger {
    wxTextCtrl* log_ctrl_;
    std::stringstream buffer_;
    std::streambuf* old_cout_;
    std::streambuf* old_cerr_;
public:
    GuiLogger(wxTextCtrl* ctrl) : log_ctrl_(ctrl) {
        old_cout_ = std::cout.rdbuf(buffer_.rdbuf());
        old_cerr_ = std::cerr.rdbuf(buffer_.rdbuf());
    }
    ~GuiLogger() {
        std::cout.rdbuf(old_cout_);
        std::cerr.rdbuf(old_cerr_);
        log_ctrl_->AppendText(buffer_.str());
    }
};

// In event handler:
void MyFrame::OnListJobs(wxCommandEvent& event) {
    GuiLogger logger(m_outputLog);
    listAllJobs();
}
```

494. **Add Tooltips:** Add `wxToolTip` to UI elements in the `submission_client_desktop` to explain what each button and text field does. This improves user experience.

495. **Standardize `CMakeLists.txt`:** The various `CMakeLists.txt` files have different styles and methods. For example, some use `find_package` while others use `add_subdirectory` for dependencies. They should be standardized. Using `find_package` with package configuration files or CMake's `FetchContent` module is generally the most robust and modern approach.

496. **Improve `getFFmpegCapabilities` Robustness:** The use of `grep`, `awk`, `tr`, and `sed` in a pipeline is very fragile and platform-dependent. A more robust C++ solution would be to execute `ffmpeg -encoders` and parse the output line by line within the C++ code, which avoids shell dependencies and is more portable.

497. **Clarify `job_size`:** The concept of `job_size` is used in the scheduling logic but it's not clear what it represents (e.g., file size in MB, video duration in seconds). This should be clearly defined in the protocol and UI. Using video duration is often a better proxy for transcoding complexity than file size.

498. **Centralize Version Information:** Create a `version.h.in` file that is processed by CMake's `configure_file` command. This would allow a single version number in the root `CMakeLists.txt` to be compiled into all executables, which can then be exposed via a `--version` flag or API endpoint.

499. **Missing `.gitignore`:** A comprehensive `.gitignore` file is crucial for any project using Git and a build system. It should ignore build directories, CMake user files, editor-specific files, and local configuration files.

500. **API Security: Don't echo back error details by default:** In `dispatch_server_cpp/main.cpp`, error messages like `"Invalid JSON: " + std::string(e.what())` are sent to the client. In a production environment, this can leak internal implementation details. It's better to log the detailed error server-side and send a generic error message with a unique request ID to the client.
