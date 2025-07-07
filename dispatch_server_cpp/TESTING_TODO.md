# Dispatch Server Unit Test Plan (150 Tests)

This document outlines the unit tests to be implemented for the C++ Transcoding Dispatch Server.

## I. API Endpoint Tests (50 Tests)

### A. `POST /jobs/` (Submit Job)
1.  [x] Test: Submit a valid job with all required fields. Expect HTTP 200 and a valid job JSON object in response.
2.  [x] Test: Submit a job with a missing `source_url`. Expect HTTP 400.
3.  [x] Test: Submit a job with a missing `target_codec`. Expect HTTP 400.
4.  [x] Test: Submit a job with an invalid JSON body. Expect HTTP 400.
5.  [x] Test: Submit a job with an empty JSON body. Expect HTTP 400.
6.  [x] Test: Submit a job with extra, unexpected fields. Expect HTTP 200 and the extra fields to be ignored.
7.  [x] Test: Submit a job with a non-string `source_url`. Expect HTTP 400 or for the server to handle type coercion gracefully.
8.  [x] Test: Submit a job with a non-string `target_codec`. Expect HTTP 400.
9.  [x] Test: Submit a job with a non-numeric `job_size`. Expect HTTP 400.
10. [x] Test: Submit a job with a non-integer `max_retries`. Expect HTTP 400.
11. [x] Test: Submit a job without an API key when one is required. Expect HTTP 401.
12. [x] Test: Submit a job with an incorrect API key. Expect HTTP 401.
13. [x] Test: Submit a job with a valid API key. Expect HTTP 200.
14. [x] Test: Verify the `job_id` created is unique across multiple quick submissions.
15. [x] Test: Verify the job status is correctly initialized to "pending".

### B. `GET /jobs/{job_id}` (Get Job Status)
16. [x] Test: Request status for a valid, existing job ID. Expect HTTP 200 and the correct job JSON.
17. [x] Test: Request status for a non-existent job ID. Expect HTTP 404.
18. [x] Test: Request status for a malformed job ID. Expect HTTP 404 or appropriate routing failure.
19. [x] Test: Request status without an API key when one is required. Expect HTTP 401.
20. [x] Test: Request status with an incorrect API key. Expect HTTP 401.

### C. `GET /jobs/` (List All Jobs)
21. [x] Test: List all jobs when no jobs exist. Expect HTTP 200 and an empty JSON array.
22. [x] Test: List all jobs when one job exists. Expect HTTP 200 and a JSON array with one job object.
23. [x] Test: List all jobs when multiple jobs exist. Expect HTTP 200 and a JSON array with all job objects.
24. [x] Test: List jobs without an API key when one is required. Expect HTTP 401.
25. [x] Test: List jobs with an incorrect API key. Expect HTTP 401.

### D. `POST /engines/heartbeat`
26. [x] Test: Send a valid heartbeat from a new engine. Expect HTTP 200 and the engine to be added to the database.
27. [x] Test: Send a valid heartbeat from an existing engine. Expect HTTP 200 and the engine's data to be updated.
28. [x] Test: Send a heartbeat with invalid JSON. Expect HTTP 400.
29. [x] Test: Send a heartbeat with a missing `engine_id`. Expect HTTP 400 or server-side error.
30. [x] Test: Send a heartbeat with a non-string `engine_id`. Expect HTTP 400.
31. [x] Test: Send a heartbeat without an API key when one is required. Expect HTTP 401.
32. [x] Test: Send a heartbeat with an incorrect API key. Expect HTTP 401.

### E. `GET /engines/` (List All Engines)
33. [x] Test: List all engines when no engines exist. Expect HTTP 200 and an empty JSON array.
34. [x] Test: List all engines when one engine exists. Expect HTTP 200 and a JSON array with one engine object.
35. [x] Test: List all engines when multiple engines exist. Expect HTTP 200 and a JSON array with all engine objects.
36. [x] Test: List engines without an API key when one is required. Expect HTTP 401.
37. [x] Test: List engines with an incorrect API key. Expect HTTP 401.

### F. `POST /jobs/{job_id}/complete`
38. [x] Test: Mark a valid, assigned job as complete. Expect HTTP 200.
39. [x] Test: Mark a non-existent job as complete. Expect HTTP 404.
40. [x] Test: Mark a job as complete with a missing `output_url`. Expect HTTP 400.
41. [x] Test: Mark a job as complete with invalid JSON. Expect HTTP 400.
42. [x] Test: Mark a job as complete without an API key when required. Expect HTTP 401.

### G. `POST /jobs/{job_id}/fail`
43. [x] Test: Mark a valid, assigned job as failed. Expect HTTP 200.
44. [x] Test: Mark a non-existent job as failed. Expect HTTP 404.
45. [x] Test: Mark a job as failed with a missing `error_message`. Expect HTTP 400.
46. [x] Test: Mark a job as failed with invalid JSON. Expect HTTP 400.
47. [x] Test: Mark a job as failed without an API key when required. Expect HTTP 401.

### H. `POST /assign_job/`
48. [x] Test: Request a job when jobs are pending and engines are idle. Expect HTTP 200 and a job object.
49. [x] Test: Request a job when no jobs are pending. Expect HTTP 204.
50. [x] Test: Request a job when jobs are pending but no engines are idle. Expect HTTP 204.

## II. State Management & Logic (50 Tests)

### A. Job State Transitions
51. [x] Test: Job is `pending` after submission.
52. [x] Test: Job becomes `assigned` after `POST /assign_job/`.
53. [x] Test: Job becomes `completed` after `POST /jobs/{job_id}/complete`.
54. [x] Test: Job becomes `pending` again after `POST /jobs/{job_id}/fail` if retries are available.
55. [x] Test: Job becomes `failed_permanently` after `POST /jobs/{job_id}/fail` if retries are exhausted.
56. [x] Test: A `completed` job cannot be marked as failed.
57. [x] Test: A `failed_permanently` job cannot be marked as failed again.
58. [x] Test: A `completed` job is not returned by `POST /assign_job/`.
59. [x] Test: A `failed_permanently` job is not returned by `POST /assign_job/`.
60. [x] Test: An `assigned` job is not returned by `POST /assign_job/`.

### B. Engine State Transitions
61. [x] Test: Engine status is `idle` after a heartbeat.
62. [x] Test: Engine status becomes `busy` after being assigned a job.
63. [x] Test: Engine status becomes `idle` again after the job it was assigned is marked `completed`.
64. [x] Test: Engine status becomes `idle` again after the job it was assigned is marked `failed`.
65. [x] Test: A `busy` engine is not assigned another job.

### C. Job Resubmission Logic
66. [x] Test: A failed job has its `retries` count incremented.
67. [x] Test: A failed job with `retries` < `max_retries` is re-queued (status `pending`).
68. [x] Test: A failed job with `retries` == `max_retries` becomes `failed_permanently`.
69. [x] Test: `max_retries` defaults to 3 if not provided in the submission.
70. [x] Test: `max_retries` can be set to 0.
71. [x] Test: A job with `max_retries`=0 that fails goes to `failed_permanently` immediately.

### D. Scheduling Logic
72. [x] Test: Scheduler assigns a pending job to an idle engine.
73. [x] Test: Scheduler does not assign a job if no engines are registered.
74. [x] Test: Scheduler does not assign a job if all registered engines are busy.
75. [x] Test: Scheduler correctly assigns a large job to a streaming-capable engine if available.
76. [x] Test: Scheduler assigns a large job to the fastest non-streaming engine if no streaming engines are available.
77. [x] Test: Scheduler assigns a small job to the slowest engine.
78. [x] Test: Scheduler assigns a medium job to the fastest engine.
79. [x] Test: Scheduler handles engines without benchmark data correctly (e.g., ignores them or uses a default).
80. [x] Test: Scheduler correctly updates engine status to `busy` upon assignment.
81. [x] Test: Scheduler correctly updates job status to `assigned` upon assignment.
82. [x] Test: Scheduler correctly records the `assigned_engine` ID in the job data.

### E. Persistent Storage (`load_state`/`save_state`)
83. [x] Test: `save_state` correctly writes the current jobs and engines to the JSON file.
84. [x] Test: `load_state` correctly loads jobs from the JSON file on startup.
85. [x] Test: `load_state` correctly loads engines from the JSON file on startup.
86. [x] Test: `load_state` handles a non-existent file gracefully (starts with empty state).
87. [x] Test: `load_state` handles a corrupt/invalid JSON file gracefully.
88. [x] Test: `load_state` handles an empty JSON file gracefully.
89. [x] Test: State is preserved after a simulated server restart (call `save_state` then `load_state`).
90. [x] Test: Submitting a new job triggers `save_state`.
91. [x] Test: A heartbeat triggers `save_state`.
92. [x] Test: Completing a job triggers `save_state`.
93. [x] Test: Failing a job triggers `save_state`.
94. [x] Test: Assigning a job triggers `save_state`.
95. [x] Test: `save_state` writes to a temporary file and renames to prevent corruption.
96. [x] Test: `load_state` correctly handles empty "jobs" or "engines" keys in the JSON file.
97. [x] Test: `save_state` produces a well-formatted (indented) JSON file.
98. [x] Test: `load_state` handles a file with only a "jobs" key.
99. [x] Test: `load_state` handles a file with only an "engines" key.
100. [x] Test: `save_state` correctly handles special characters in job/engine data.

## III. Core C++ & Miscellaneous (50 Tests)

### A. Command-Line Arguments
101. [x] Test: Server starts with no arguments.
102. [x] Test: Server correctly parses the `--api-key` argument.
103. [x] Test: Server ignores unknown command-line arguments.
104. [x] Test: Server handles `--api-key` without a value gracefully.

### B. Thread Safety
105. [x] Test: Submit multiple jobs concurrently from different threads. Verify all are added correctly.
106. [x] Test: Send multiple heartbeats concurrently from different threads. Verify all engines are updated correctly.
107. [x] Test: Concurrently submit jobs and send heartbeats. Verify data integrity.
108. [x] Test: Concurrently assign jobs and complete jobs. Verify state transitions are correct.
109. [x] Test: Access the main jobs database (`jobs_db`) from multiple threads with proper locking.
110. [x] Test: Access the main engines database (`engines_db`) from multiple threads with proper locking.

### C. Helper Functions & Utilities
111. [x] Test: `load_state` with a file containing a single job.
112. [x] Test: `load_state` with a file containing a single engine.
113. [x] Test: `save_state` with a single job.
114. [x] Test: `save_state` with a single engine.
115. [x] Test: `save_state` with zero jobs and zero engines.
116. [x] Test: `load_state` from a file created by `save_state` with zero jobs/engines.
117. [x] Test: Job ID generation is sufficiently random to avoid collisions in a tight loop.
118. [x] Test: JSON parsing of a valid job submission request.
119. [x] Test: JSON parsing of a valid heartbeat request.
120. [x] Test: JSON parsing of a valid job completion request.
121. [ ] Test: JSON parsing of a job failure request.

### D. Edge Cases & Error Handling
122. [ ] Test: Server handles a client disconnecting mid-request.
123. [ ] Test: Server handles extremely long `source_url` values.
124. [ ] Test: Server handles extremely long `engine_id` values.
125. [ ] Test: Server handles a very large number of jobs in the database.
126. [ ] Test: Server handles a very large number of engines in the database.
127. [ ] Test: Server handles a job ID that looks like a number but is a string.
128. [ ] Test: Server handles a heartbeat for an engine that was assigned a job that no longer exists.
129. [ ] Test: Server handles a request to complete a job that was never assigned.
130. [ ] Test: Server handles a request to fail a a job that was never assigned.
131. [ ] Test: Server handles a job submission with a negative `job_size`.
132. [ ] Test: Server handles a job submission with a negative `max_retries`.
133. [ ] Test: Server handles a heartbeat with a negative `storage_capacity_gb`.
134. [ ] Test: Server handles a heartbeat with a non-boolean `streaming_support`.
135. [ ] Test: Server handles a benchmark result for a non-existent engine.
136. [ ] Test: Server handles a benchmark result with a negative `benchmark_time`.
137. [ ] Test: Server handles a request to `/jobs/` with a trailing slash.
138. [ ] Test: Server handles a request to `/engines/` with a trailing slash.
139. [ ] Test: Server handles a request with a non-standard `Content-Type` header.
140. [ ] Test: Server handles a request with a very large request body (should be rejected).

### E. Refactoring-Specific Tests
141. [ ] Test: (Refactor) `run_dispatch_server` can be called without `argc`/`argv`.
142. [ ] Test: (Refactor) `API_KEY` can be set programmatically, not just from `argv`.
143. [ ] Test: (Refactor) The `httplib::Server` instance can be accessed from tests.
144. [ ] Test: (Refactor) `jobs_db` can be cleared and pre-populated for a test.
145. [ ] Test: (Refactor) `engines_db` can be cleared and pre-populated for a test.
146. [ ] Test: (Refactor) `PERSISTENT_STORAGE_FILE` can be pointed to a temporary file for a test.
147. [ ] Test: (Refactor) The main server loop can be started and stopped programmatically from a test.
148. [ ] Test: (Refactor) The server can be started on a random available port to allow parallel test execution.
149. [ ] Test: (Refactor) `save_state` can be mocked to prevent actual file I/O during tests.
150. [ ] Test: (Refactor) `load_state` can be mocked to provide a specific state for a test.
