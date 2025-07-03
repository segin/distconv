# Submission Client Unit Test Plan (150 Tests)

This document outlines the unit tests to be implemented for the C++ wxWidgets Submission Client.

## I. Core API Client Logic (50 Tests)

### A. `submitJob`
1.  [x] Test: `submitJob` sends a POST request to the correct URL (`/jobs/`).
2.  [x] Test: `submitJob` correctly serializes the JSON payload with `source_url`.
3.  [x] Test: `submitJob` correctly serializes the JSON payload with `target_codec`.
4.  [x] Test: `submitJob` correctly serializes the JSON payload with `job_size`.
5.  [x] Test: `submitJob` correctly serializes the JSON payload with `max_retries`.
6.  [x] Test: `submitJob` includes the `X-API-Key` header.
7.  [x] Test: `submitJob` includes the `Content-Type: application/json` header.
8.  [x] Test: `submitJob` handles a successful (HTTP 200) response.
9.  [x] Test: `submitJob` handles a server error (e.g., HTTP 500) response.
10. [x] Test: `submitJob` handles a client error (e.g., HTTP 400) response.
11. [x] Test: `submitJob` handles a transport error (e.g., connection refused).
12. [x] Test: `submitJob` correctly parses the job ID from a successful response.
13. [x] Test: `submitJob` calls `saveJobId` on success.
14. [x] Test: `submitJob` uses SSL verification when a CA path is provided.
15. [x] Test: `submitJob` disables SSL verification when no CA path is provided.

### B. `getJobStatus`
16. [x] Test: `getJobStatus` sends a GET request to the correct URL (`/jobs/{job_id}`).
17. [x] Test: `getJobStatus` includes the `X-API-Key` header.
18. [x] Test: `getJobStatus` handles a successful (HTTP 200) response.
19. [ ] Test: `getJobStatus` handles a not found (HTTP 404) response.
20. [ ] Test: `getJobStatus` handles a server error (e.g., HTTP 500) response.
21. [ ] Test: `getJobStatus` handles a transport error.
22. [ ] Test: `getJobStatus` correctly parses and returns the JSON response.
23. [ ] Test: `getJobStatus` uses SSL verification when a CA path is provided.
24. [ ] Test: `getJobStatus` disables SSL verification when no CA path is provided.

### C. `listAllJobs`
25. [ ] Test: `listAllJobs` sends a GET request to the correct URL (`/jobs/`).
26. [ ] Test: `listAllJobs` includes the `X-API-Key` header.
27. [ ] Test: `listAllJobs` handles a successful (HTTP 200) response.
28. [ ] Test: `listAllJobs` handles a server error (e.g., HTTP 500) response.
29. [ ] Test: `listAllJobs` handles a transport error.
30. [ ] Test: `listAllJobs` correctly parses and returns the JSON array response.
31. [ ] Test: `listAllJobs` uses SSL verification when a CA path is provided.
32. [ ] Test: `listAllJobs` disables SSL verification when no CA path is provided.

### D. `listAllEngines`
33. [ ] Test: `listAllEngines` sends a GET request to the correct URL (`/engines/`).
34. [ ] Test: `listAllEngines` includes the `X-API-Key` header.
35. [ ] Test: `listAllEngines` handles a successful (HTTP 200) response.
36. [ ] Test: `listAllEngines` handles a server error (e.g., HTTP 500) response.
37. [ ] Test: `listAllEngines` handles a transport error.
38. [ ] Test: `listAllEngines` correctly parses and returns the JSON array response.
39. [ ] Test: `listAllEngines` uses SSL verification when a CA path is provided.
40. [ ] Test: `listAllEngines` disables SSL verification when no CA path is provided.

### E. Local Job ID Storage
41. [ ] Test: `saveJobId` writes a job ID to the specified file.
42. [ ] Test: `saveJobId` appends a new job ID to an existing file.
43. [ ] Test: `loadJobIds` returns an empty vector if the file doesn't exist.
44. [ ] Test: `loadJobIds` returns a vector with one ID from a file with one line.
45. [ ] Test: `loadJobIds` returns a vector with multiple IDs from a file with multiple lines.
46. [ ] Test: `loadJobIds` handles an empty file.
47. [ ] Test: `saveJobId` handles a case where the file cannot be opened for writing.
48. [ ] Test: `loadJobIds` handles a job ID with special characters.
49. [ ] Test: `saveJobId` and `loadJobIds` work correctly together.
50. [ ] Test: The `JOB_IDS_FILE` constant has the correct value.

## II. GUI Logic & Event Handlers (50 Tests)

### A. `MyFrame` Initialization
51. [ ] Test: The main frame is created with the correct title.
52. [ ] Test: The `wxPanel` is created.
53. [ ] Test: All text controls (`m_sourceUrlText`, etc.) are created and not null.
54. [ ] Test: All buttons (`submitButton`, etc.) are created and not null.
55. [ ] Test: The output log (`m_outputLog`) is created and is read-only.
56. [ ] Test: Sizers are correctly attached to the panel.
57. [ ] Test: The frame is centered on the screen.

### B. `OnSubmit` Event Handler
58. [ ] Test: `OnSubmit` reads the value from `m_sourceUrlText`.
59. [ ] Test: `OnSubmit` reads the value from `m_targetCodecText`.
60. [ ] Test: `OnSubmit` reads the value from `m_jobSizeText`.
61. [ ] Test: `OnSubmit` reads the value from `m_maxRetriesText`.
62. [ ] Test: `OnSubmit` shows an error if `source_url` is empty.
63. [ ] Test: `OnSubmit` shows an error if `target_codec` is empty.
64. [ ] Test: `OnSubmit` shows an error if `job_size` is empty.
65. [ ] Test: `OnSubmit` uses a default value for `max_retries` if the field is empty.
66. [ ] Test: `OnSubmit` correctly converts `job_size` from string to double.
67. [ ] Test: `OnSubmit` correctly converts `max_retries` from string to int.
68. [ ] Test: `OnSubmit` shows an error for invalid (non-numeric) `job_size`.
69. [ ] Test: `OnSubmit` shows an error for invalid (non-numeric) `max_retries`.
70. [ ] Test: `OnSubmit` calls the `submitJob` API function with the correct parameters.
71. [ ] Test: `OnSubmit` captures and displays output from the API call in the log.
72. [ ] Test: `OnSubmit` captures and displays error output from the API call in the log.

### C. `OnGetStatus` Event Handler
73. [ ] Test: `OnGetStatus` reads the value from `m_jobIdText`.
74. [ ] Test: `OnGetStatus` shows an error if `job_id` is empty.
75. [ ] Test: `OnGetStatus` calls the `getJobStatus` API function with the correct job ID.
76. [ ] Test: `OnGetStatus` captures and displays output from the API call in the log.

### D. `OnListJobs` Event Handler
77. [ ] Test: `OnListJobs` calls the `listAllJobs` API function.
78. [ ] Test: `OnListJobs` captures and displays output from the API call in the log.

### E. `OnListEngines` Event Handler
79. [ ] Test: `OnListEngines` calls the `listAllEngines` API function.
80. [ ] Test: `OnListEngines` captures and displays output from the API call in the log.

### F. `OnRetrieveLocalJobs` Event Handler
81. [ ] Test: `OnRetrieveLocalJobs` calls `loadJobIds`.
82. [ ] Test: `OnRetrieveLocalJobs` displays a message if no local IDs are found.
83. [ ] Test: `OnRetrieveLocalJobs` calls `getJobStatus` for each loaded ID.
84. [ ] Test: `OnRetrieveLocalJobs` displays the list of local job IDs.
85. [ ] Test: `OnRetrieveLocalJobs` captures and displays all output in the log.

### G. GUI State & Configuration
86. [ ] Test: `g_dispatchServerUrl` has the correct default value.
87. [ ] Test: `g_apiKey` has the correct default value.
88. [ ] Test: `g_caCertPath` has the correct default value.
89. [ ] Test: (Refactor) A configuration dialog can be opened.
90. [ ] Test: (Refactor) The configuration dialog correctly loads the current global values.
91. [ ] Test: (Refactor) Saving the configuration dialog updates the global variables.
92. [ ] Test: (Refactor) The client uses the updated global variables for subsequent API calls.
93. [ ] Test: (Refactor) Configuration is saved to and loaded from a file.

### H. General UI Behavior
94. [ ] Test: The `OnExit` handler closes the application.
95. [ ] Test: The `MyApp::OnInit` function creates and shows the `MyFrame`.
96. [ ] Test: The output log is cleared before a new command is run.
97. [ ] Test: The UI remains responsive during a long API call (requires async implementation).
98. [ ] Test: The job list UI element is populated by `OnListJobs`.
99. [ ] Test: The engine list UI element is populated by `OnListEngines`.
100. [ ] Test: Selecting a job in the list displays its details in a separate panel.

## III. C++ Idioms, Mocks, & Refactoring (50 Tests)

### A. API Client Refactoring
101. [ ] Test: (Refactor) All `cpr` calls are moved into a dedicated `ApiClient` class.
102. [ ] Test: (Refactor) The `ApiClient` class is initialized with the server URL and API key.
103. [ ] Test: (Refactor) The `MyFrame` class holds an instance of the `ApiClient`.
104. [ ] Test: (Refactor) `submitJob` in `ApiClient` returns a result object, not `void`.
105. [ ] Test: (Refactor) `getJobStatus` in `ApiClient` returns a result object.
106. [ ] Test: (Refactor) The result object contains success/failure status, response data, and error messages.
107. [ ] Test: (Refactor) The GUI event handlers call methods on the `ApiClient` instance.
108. [ ] Test: (Refactor) The GUI updates itself based on the contents of the result object.
109. [ ] Test: (Refactor) The global functions (`submitJob`, `getJobStatus`, etc.) are removed.
110. [ ] Test: (Refactor) The global configuration variables (`g_dispatchServerUrl`, etc.) are moved into the `ApiClient` or a `Config` class.

### B. Asynchronous Operations
111. [ ] Test: (Refactor) `ApiClient` methods are asynchronous and take a callback function.
112. [ ] Test: (Refactor) The callback is executed on the main UI thread.
113. [ ] Test: (Refactor) The UI shows a "loading" indicator while an API call is in progress.
114. [ ] Test: (Refactor) The UI correctly updates after the async callback is executed.
115. [ ] Test: (Refactor) Multiple API calls can be made in parallel without blocking the UI.

### C. Mocking & Testability
116. [ ] Test: The `ApiClient` can be tested without making real HTTP calls.
117. [ ] Test: A mock HTTP client can be injected into the `ApiClient`.
118. [ ] Test: `submitJob` can be tested with a mocked successful response.
119. [ ] Test: `submitJob` can be tested with a mocked failure response.
120. [ ] Test: `getJobStatus` can be tested with a mocked successful response.
121. [ ] Test: `getJobStatus` can be tested with a mocked 404 response.
122. [ ] Test: The GUI event handlers can be tested without a running `wxApp`.
123. [ ] Test: `OnSubmit` can be tested by mocking the `ApiClient` call.
124. [ ] Test: `OnGetStatus` can be tested by mocking the `ApiClient` call.
125. [ ] Test: `loadJobIds` can be tested by providing a temporary file path.
126. [ ] Test: `saveJobId` can be tested by checking the contents of a temporary file.

### D. Code Quality & Edge Cases
127. [ ] Test: The `cout`/`cerr` redirection is removed in favor of the `ApiClient` result object.
128. [ ] Test: All `std::stod` and `std::stoi` calls are protected against exceptions.
129. [ ] Test: The client handles a server response with invalid JSON gracefully.
130. [ ] Test: The client handles a server response with missing expected fields.
131. [ ] Test: The client handles an extremely long job ID input.
132. [ ] Test: The client handles an extremely long source URL input.
133. [ ] Test: The client handles a very large list of jobs from the server.
134. [ ] Test: The client handles a very large list of engines from the server.
135. [ ] Test: The code compiles with `-Wall -Wextra -pedantic` with zero warnings.
136. [ ] Test: The code is formatted with `clang-format`.
137. [ ] Test: `cpr::VerifySsl` is correctly constructed from the `g_caCertPath` string.
138. [ ] Test: The `OnSubmit` handler correctly deals with unicode characters in text fields.
139. [ ] Test: The `OnGetStatus` handler correctly deals with unicode characters in the job ID field.
140. [ ] Test: The client handles the `submitted_job_ids.txt` file being read-only.
141. [ ] Test: The client handles the `submitted_job_ids.txt` file being a directory.
142. [ ] Test: The client handles the server URL having a trailing slash.
143. [ ] Test: The client handles the server URL not having a trailing slash.
144. [ ] Test: The client sets a reasonable timeout for all `cpr` requests.
145. [ ] Test: The client correctly parses a JSON response that is an array of objects.
146. [ ] Test: The client correctly parses a JSON response that is a single object.
147. [ ] Test: The client handles a server response with a `Content-Type` other than `application/json`.
148. [ ] Test: The client UI layout doesn't break on window resize.
149. [ ] Test: The client handles an API key with special characters.
150. [ ] Test: The client handles a server URL with a non-standard port.
