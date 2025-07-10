# Transcoding Engine Unit Test Plan (150 Tests)

This document outlines the unit tests to be implemented for the C++ Transcoding Engine.

## I. Core Logic & State (50 Tests)

### A. Initialization & Configuration
1.  [x] Test: Engine starts with default configuration values.
2.  [x] Test: Engine correctly parses `--ca-cert` command-line argument.
3.  [x] Test: Engine correctly parses `--dispatch-url` command-line argument.
4.  [x] Test: Engine correctly parses `--api-key` command-line argument.
5.  [x] Test: Engine correctly parses `--hostname` command-line argument.
6.  [x] Test: Engine ignores unknown command-line arguments.
7.  [x] Test: `engineId` is generated and is not empty.
8.  [x] Test: `engineId` is reasonably unique on subsequent runs.
9.  [x] Test: `getHostname` returns a non-empty string.
10. [x] Test: `init_sqlite` creates the database file if it doesn't exist.
11. [x] Test: `init_sqlite` creates the `jobs` table if it doesn't exist.
12. [x] Test: `init_sqlite` handles an existing database and table without error.
13. [x] Test: `get_jobs_from_db` returns an empty vector when the database is new.

### B. Main Loop & Job Polling
14. [ ] Test: `getJob` function is called periodically in the main loop.
15. [ ] Test: `performTranscoding` is called when `getJob` returns a valid job.
16. [ ] Test: `performStreamingTranscoding` is called when a job requires it (future feature).
17. [ ] Test: The main loop continues to poll after a job is processed.
18. [ ] Test: The main loop continues to poll if `getJob` returns an empty string.
19. [ ] Test: The main loop handles a malformed JSON response from `getJob` gracefully.

### C. Transcoding Process (`performTranscoding`)
20. [ ] Test: `downloadFile` is called with the correct URL and output path.
21. [ ] Test: `ffmpeg` command is constructed with the correct input file and codec.
22. [ ] Test: `system()` call to `ffmpeg` is executed.
23. [ ] Test: `uploadFile` is called with the correct file path.
24. [ ] Test: `reportJobStatus` is called with `completed` on success.
25. [ ] Test: `reportJobStatus` is called with `failed` if `downloadFile` fails.
26. [ ] Test: `reportJobStatus` is called with `failed` if `ffmpeg` command returns a non-zero exit code.
27. [ ] Test: `reportJobStatus` is called with `failed` if `uploadFile` fails.
28. [ ] Test: Temporary input file is deleted after transcoding.
29. [ ] Test: Temporary output file is deleted after transcoding.
30. [ ] Test: The function handles a missing `job_id` in the JSON gracefully.
31. [ ] Test: The function handles a missing `source_url` in the JSON gracefully.
32. [ ] Test: The function handles a missing `target_codec` in the JSON gracefully.

### D. SQLite Job Queue
33. [ ] Test: `add_job_to_db` correctly inserts a job ID.
34. [ ] Test: `add_job_to_db` does not insert a duplicate job ID.
35. [ ] Test: `remove_job_from_db` correctly removes a job ID.
36. [ ] Test: `remove_job_from_db` handles a non-existent job ID without error.
37. [ ] Test: `get_jobs_from_db` correctly retrieves all job IDs.
38. [ ] Test: The in-memory `localJobQueue` vector is correctly populated from the DB at startup.
39. [ ] Test: A job ID is added to the in-memory queue when `add_job_to_db` is called.
40. [ ] Test: A job ID is removed from the in-memory queue when `remove_job_from_db` is called.
41. [ ] Test: The SQLite `callback` function correctly parses and adds a job ID to the vector.

### E. Resource Monitoring
42. [ ] Test: `getCpuTemperature` returns a plausible value on Linux (if run on Linux).
43. [ ] Test: `getCpuTemperature` returns -1.0 on unsupported OS.
44. [ ] Test: `getFFmpegCapabilities` for `encoders` returns a non-empty string.
45. [ ] Test: `getFFmpegCapabilities` for `decoders` returns a non-empty string.
46. [ ] Test: `getFFmpegHWAccels` returns a non-empty string.
47. [ ] Test: `getFFmpegCapabilities` handles `popen` failure gracefully (returns empty string).
48. [ ] Test: `performBenchmark` returns a positive duration.
49. [ ] Test: `performBenchmark` runs for approximately the correct amount of time.
50. [ ] Test: `getHostname` handles `gethostname` system call failure.

## II. Network Communication (50 Tests)

### A. `makeHttpRequest`
51. [ ] Test: `makeHttpRequest` performs a GET request correctly.
52. [ ] Test: `makeHttpRequest` performs a POST request with a payload correctly.
53. [ ] Test: `makeHttpRequest` sets the `Content-Type` header for POST requests.
54. [ ] Test: `makeHttpRequest` sets the `X-API-Key` header when an API key is provided.
55. [ ] Test: `makeHttpRequest` does not set the `X-API-Key` header when the key is empty.
56. [ ] Test: `makeHttpRequest` handles an invalid URL gracefully.
57. [ ] Test: `makeHttpRequest` handles a server that is not running.
58. [ ] Test: `makeHttpRequest` correctly returns the response body.
59. [ ] Test: `makeHttpRequest` enables SSL verification when a CA path is provided.
60. [ ] Test: `makeHttpRequest` disables SSL verification when no CA path is provided.
61. [ ] Test: `makeHttpRequest` handles a `curl_easy_init` failure.
62. [ ] Test: `makeHttpRequest` handles a `curl_easy_perform` failure.

### B. `sendHeartbeat`
63. [ ] Test: `sendHeartbeat` constructs the correct URL.
64. [ ] Test: `sendHeartbeat` constructs a valid JSON payload.
65. [ ] Test: `sendHeartbeat` includes the correct `engine_id` in the payload.
66. [ ] Test: `sendHeartbeat` includes the correct `hostname` in the payload.
67. [ ] Test: `sendHeartbeat` includes the correct `storage_capacity_gb`.
68. [ ] Test: `sendHeartbeat` includes `streaming_support` as true.
69. [ ] Test: `sendHeartbeat` includes `streaming_support` as false.
70. [ ] Test: `sendHeartbeat` includes non-empty encoder/decoder/hwaccel strings.
71. [ ] Test: `sendHeartbeat` calls `makeHttpRequest` with the correct parameters.

### C. `reportJobStatus`
72. [ ] Test: `reportJobStatus` for `completed` constructs the correct URL.
73. [ ] Test: `reportJobStatus` for `completed` constructs a valid JSON payload with `output_url`.
74. [ ] Test: `reportJobStatus` for `failed` constructs the correct URL.
75. [ ] Test: `reportJobStatus` for `failed` constructs a valid JSON payload with `error_message`.
76. [ ] Test: `reportJobStatus` calls `makeHttpRequest` with the correct parameters.
77. [ ] Test: `reportJobStatus` handles an empty `output_url`.
78. [ ] Test: `reportJobStatus` handles an empty `error_message`.

### D. `sendBenchmarkResult`
79. [ ] Test: `sendBenchmarkResult` constructs the correct URL.
80. [ ] Test: `sendBenchmarkResult` constructs a valid JSON payload.
81. [ ] Test: `sendBenchmarkResult` includes the correct `engine_id` and `benchmark_time`.
82. [ ] Test: `sendBenchmarkResult` calls `makeHttpRequest` with the correct parameters.

### E. `getJob`
83. [ ] Test: `getJob` constructs the correct URL.
84. [ ] Test: `getJob` constructs a valid JSON payload with the `engine_id`.
85. [ ] Test: `getJob` calls `makeHttpRequest` with the correct parameters.
86. [ ] Test: `getJob` returns the response body from `makeHttpRequest`.

### F. `downloadFile`
87. [ ] Test: `downloadFile` attempts to open the output file for writing.
88. [ ] Test: `downloadFile` calls `curl_easy_perform`.
89. [ ] Test: `downloadFile` returns true on success.
90. [ ] Test: `downloadFile` returns false on curl failure.
91. [ ] Test: `downloadFile` returns false if the output file cannot be opened.
92. [ ] Test: `downloadFile` enables SSL verification with a CA path.
93. [ ] Test: `downloadFile` disables SSL verification without a CA path.

### G. `uploadFile` (Placeholder)
94. [ ] Test: `uploadFile` placeholder function returns true.
95. [ ] Test: `uploadFile` prints the correct placeholder message.

### H. Threading
96. [ ] Test: `heartbeatThread` is successfully created and detached.
97. [ ] Test: `benchmarkThread` is successfully created and detached.
98. [ ] Test: The main thread continues execution after detaching worker threads.
99. [ ] Test: (Mocking) `sendHeartbeat` is called from within the heartbeat thread.
100. [ ] Test: (Mocking) `sendBenchmarkResult` is called from within the benchmark thread.

## III. C++ Idioms, Mocks, & Refactoring (50 Tests)

### A. `cJSON` to `nlohmann/json` Migration
101. [ ] Test: (Refactor) `sendHeartbeat` payload is built using `nlohmann::json`.
102. [ ] Test: (Refactor) `localJobQueue` in heartbeat is a JSON array, not a string.
103. [ ] Test: (Refactor) `getJob` response is parsed using `nlohmann::json`.
104. [ ] Test: (Refactor) `nlohmann::json::parse` handles an invalid job JSON from the server.
105. [ ] Test: (Refactor) `nlohmann::json::value` is used for safe access to optional fields.
106. [ ] Test: Remove `cJSON` dependency from `CMakeLists.txt`.
107. [ ] Test: Remove `#include "cjson/cJSON.h"`.

### B. `libcurl` to `cpr` Migration
108. [ ] Test: (Refactor) `makeHttpRequest` is replaced with `cpr` calls.
109. [ ] Test: (Refactor) `sendHeartbeat` uses `cpr::Post`.
110. [ ] Test: (Refactor) `getJob` uses `cpr::Post`.
111. [ ] Test: (Refactor) `downloadFile` uses `cpr::Download`.
112. [ ] Test: (Refactor) `cpr` calls correctly set the API key header.
113. [ ] Test: (Refactor) `cpr` calls correctly set the request body.
114. [ ] Test: (Refactor) `cpr` calls handle server errors (e.g., 500) correctly.
115. [ ] Test: (Refactor) `cpr` calls handle transport errors (e.g., no connection) correctly.
116. [ ] Test: (Refactor) `cpr` calls use `cpr::VerifySsl(false)` when no CA path is given.
117. [ ] Test: (Refactor) `cpr` calls use `cpr::SslOptions` when a CA path is given.
118. [ ] Test: Remove `libcurl` dependency from `CMakeLists.txt`.
119. [ ] Test: Remove `#include <curl/curl.h>`.

### C. `system()` to Secure Subprocess Replacement
120. [ ] Test: (Refactor) `performTranscoding` uses a subprocess library instead of `system()`.
121. [ ] Test: (Refactor) `ffmpeg` arguments are passed as a vector, not a single string.
122. [ ] Test: (Refactor) `ffmpeg` process can be terminated.
123. [ ] Test: (Refactor) `ffmpeg` `stdout` and `stderr` can be captured.
124. [ ] Test: (Refactor) `getFFmpegCapabilities` uses the subprocess library.
125. [ ] Test: (Refactor) The subprocess library correctly finds the `ffmpeg` executable.
126. [ ] Test: (Refactor) The subprocess library handles `ffmpeg` not being in the PATH.

### D. Mocking & Testability
127. [ ] Test: `performTranscoding` can be tested without actual file I/O (mock `downloadFile`, `uploadFile`).
128. [ ] Test: `performTranscoding` can be tested without running `ffmpeg` (mock the subprocess call).
129. [ ] Test: `sendHeartbeat` can be tested without making a real HTTP call (mock the network client).
130. [ ] Test: `getJob` can be tested by providing a mock HTTP response.
131. [ ] Test: `init_sqlite` can be pointed to an in-memory database for tests.
132. [ ] Test: `init_sqlite` can be pointed to a temporary file-based database for tests.
133. [ ] Test: The main engine loop can be run for a single iteration for testing purposes.
134. [ ] Test: The worker threads (`heartbeatThread`, `benchmarkThread`) are not started in test mode.
135. [ ] Test: `run_transcoding_engine` can be called with a special flag to run in test mode.
136. [ ] Test: All core logic is refactored out of `run_transcoding_engine` into a testable `TranscodingEngine` class.
137. [ ] Test: The `TranscodingEngine` class takes a mockable network client in its constructor.
138. [ ] Test: The `TranscodingEngine` class takes a mockable subprocess runner in its constructor.
139. [ ] Test: The `TranscodingEngine` class takes a mockable database connection in its constructor.

### E. Code Quality & Edge Cases
140. [ ] Test: All `new` and `delete` are replaced by smart pointers (`std::unique_ptr`, `std::shared_ptr`).
141. [ ] Test: Raw C-style casts are replaced with `static_cast`, `reinterpret_cast`, etc.
142. [ ] Test: The code compiles with `-Wall -Wextra -pedantic` with zero warnings.
143. [ ] Test: The code is formatted with `clang-format`.
144. [ ] Test: `sqlite3_mprintf` is replaced with prepared statements (`sqlite3_prepare_v2`, `sqlite3_bind_*`) to prevent SQL injection.
145. [ ] Test: `sqlite3_exec` error messages (`zErrMsg`) are properly freed in all code paths.
146. [ ] Test: File handles from `popen` are closed in all code paths.
147. [ ] Test: File handles from `fopen` are closed in all code paths.
148. [ ] Test: The engine handles a job ID with special characters that might be problematic for filenames.
149. [ ] Test: The engine handles a `source_url` with special characters.
150. [ ] Test: The engine handles the dispatch server returning a malformed, non-JSON error response.
