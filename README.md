# Distributed Video Transcoding Project

This document outlines the strategic plan for developing a distributed video transcoding system.

## User Prompt

This is the Gemini CLI. We are setting up the context for our chat.
  Today's date is Saturday, June 28, 2025.
  My operating system is: linux
  I'm currently working in the directory: /home/segin/distconv
  Showing up to 200 items (files + folders). Folders or files indicated with ... contain more items not shown, were ignored, or the display limit (200 items) was reached.

/home/segin/distconv/
└───.git/...

You have a blank directory. What we're doing in this directory is creating a brand-new project. The project is for the distributed transcoding of video content in order to normalize bitrates and optimize disk space usage. What I want you to
  do is to help develop a basic version of the program. There will be three parts: 1. A "Transcoding Engine". This represents the daemon running on each individual computer system that will be participating in video transcoding. 2. A "Transcoding Dispatch Server", a central server that tracks which "Transcoding Engines"'s activity, handles the storage for submitted videos waiting for transcoding as well as the final outputs. Then there are "Submission Clients" which submit transcoding jobs and retrieve the results for final local storage. The "Transcoding Engine" is almost certainly something we'd want to have in C, C++, or some other performant system langauge. It'll likely use `ffmpeg` in some capacity to actually perform the actual media transcoding. The "Transcoding Dispatch Server" can be written in any language. Investigate possible protocols for synchronizing state like HTTP REST APIs or the like. The central server should also track the local storage on the transcoding endpoints and determine if it is possible for the central server to send the entire source media file to the endpoint, or if it will be necessary to instead stream the source media to the transcoding endpoint and stream the final transcoded output back from the transcoding endpoint. The central server should also plan for the possibility that the transcoding jobs are going to fail for reasons that are transient and be willing to resubmit incomplete jobs back to compute nodes (transcoding endpoints.) Make a strategic plan document outlying the entire project roadmap, starting by copying this entire prompt into the source document and then further iterating on that document. Make sure to write testsuites and unit tests for all three parts of the project. Make sure to document any and all protocols developed for this project. Do make sure to consider Protocol Buffers in addition to HTTP, IRC, XMPP, or SIP.

## Project Roadmap

This project will be developed in phases, starting with a minimal viable product (MVP) and then iterating to add more features.

**Phase 1: Core Functionality (MVP)**

1.  **Transcoding Engine (C++):**
    *   [x] Develop a simple daemon that can accept a transcoding job.
    *   [x] The job will contain the source video location (URL) and transcoding parameters.
    *   [x] The engine will download the source video, transcode it using `ffmpeg` (initially focusing on H.264 and H.265 codecs), and make the output available for download.
    *   [x] The engine will report its status (idle, transcoding, error) to the dispatch server.
    *   [x] Integrate cJSON for parsing job details.

2.  **Transcoding Dispatch Server (C++ - NEW PRIMARY):**
    *   [x] Develop a server with a REST API.
    *   [x] The API will have endpoints for:
        *   Submitting a transcoding job.
        *   Querying the status of a job.
        *   Listing available transcoding engines.
        *   A "heartbeat" endpoint for engines to report their status.
    *   [x] The server will maintain a queue of transcoding jobs.
    *   [x] The server will assign jobs to available engines.
    *   [x] Implement configurable storage pools for transcoded content.
    *   [x] Implement persistent storage for tracking transcoding job states and submitted media.
    *   [x] Port the central server to C++.

3.  **Submission Client (C++ - NEW PRIMARY):**
    *   [x] Implement a C++ console client to submit jobs, retrieve status, list all jobs, list all engines, and retrieve locally submitted job results.
    *   [x] Enhance the C++ console client for improved user experience (interactive menu, clearer output, input validation).

**Phase 2: Advanced Features**

1.  **Streaming:**
    *   [x] Implement streaming of source video to the transcoding engine and streaming of the output back to the dispatch server. This will be used when the engine has limited local storage.
    *   [x] The dispatch server will decide whether to use streaming or file transfer based on the engine's reported storage capacity.

2.  **Job Resubmission:**
    *   [x] The dispatch server will monitor job progress.
    *   [x] If a job fails, the server will automatically resubmit it to another available engine.
    *   [x] A maximum number of retries will be configured to prevent infinite loops.

3.  **Improved Scheduling:**
    *   [x] The dispatch server will use more sophisticated logic to assign jobs, taking into account engine performance and load.
    *   [x] Implement benchmarking of transcoding endpoints. The dispatch server will trigger benchmarking tasks on engines, which will execute them and report completion speeds back to the server. This data will then be used for optimized job assignment (smaller jobs to slower systems, larger jobs to faster systems).

**Phase 3: Security and Scalability**

1.  **Security:**
    *   [x] Implement authentication and authorization for the dispatch server API.
    *   [x] Implement API key authentication for the Dispatch Server.
    *   [x] Implement API key authentication for the Dispatch Server.
    *   [ ] Encrypt communication between the components. (Acknowledged as a critical future enhancement, but beyond current MVP scope due to complexity of full TLS implementation across components.)

2.  **Scalability:**
    *   [x] Optimize the dispatch server to handle a large number of transcoding engines and jobs.
    *   [x] Consider using a more robust job queueing system (e.g., RabbitMQ, Kafka).
    *   [x] Report `ffmpeg` capabilities (encoders, decoders, hardware acceleration) from transcoding engines to the dispatch server.

## New Features (Post-MVP)

1.  **Submission Client (C++ Desktop GUI):**
    *   [ ] Implement a C++ GUI client using wxWidgets to submit jobs, retrieve status, list all jobs, list all engines, and retrieve locally submitted job results.
        *   [ ] Design and implement the main window with three tabs: "My Jobs", "All Jobs", and "Endpoints".
        *   [ ] For "My Jobs" and "All Jobs" tabs: Implement a split view with an info box for the selected job and a list of jobs.
        *   [ ] For "Endpoints" tab: Implement a split view with a list of endpoints on the left and, for the selected endpoint, a list of jobs on the selected endpoint.
        *   [ ] Implement right-click context menus for relevant items (e.g., jobs, endpoints).
        *   [ ] Implement top-level menu actions (e.g., File, Edit, View, Help).
        *   [ ] Implement a configuration dialog for Dispatch Server URL, API Key, and CA Certificate Path.
        *   [ ] Implement an "About" dialog.
        *   [ ] Implement a "Help" dialog/section.

2.  **Transcoding Engine (CPU Temperature):**
    *   [x] Implement CPU temperature reporting (Linux, FreeBSD, Windows).
    *   [x] Plan for GPU temperature reporting in the future.

3.  **Transcoding Engine (Local Job Queue):**
    *   [x] Implement local job queue reporting from transcoding engine to dispatch server.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.
    *   [x] Implement local job queue persistence using SQLite.

4.  **TLS/SSL Implementation:**
    *   [ ] Implement TLS/SSL for secure communication between all components.
        *   [ ] Generate self-signed certificates (server.key, server.crt).
        *   [ ] Implement TLS in C++ Central Server (via external proxy like `nginx`).
        *   [x] Implement TLS in C++ Transcoding Engine (using `libcurl` with `--cacert`).
        *   [x] Implement TLS in C++ Submission Client (using `cpr` with `cpr::VerifySsl`).

5.  **API Key Provisioning:**
    *   [x] Implement API key provisioning for the Transcoding Engine.

6.  **Hostname Reporting:**
    *   [x] Implement hostname reporting from the Transcoding Engine to the Dispatch Server.

7.  **Unit Testing:**
    *   [ ] Implement a comprehensive unit testing framework (e.g., Google Test).
    *   [ ] Write unit tests for the C++ submission client.
    *   [ ] Write unit tests for the C++ central server.
    *   [ ] Write unit tests for the C++ transcoding engine.
    *   [ ] Integrate Valgrind into the testing process for memory leak detection.

## Future Enhancements:

For a comprehensive list of future enhancements, bug fixes, and architectural suggestions, please refer to [SUGGESTIONS.md](SUGGESTIONS.md).


### Transcoding Engine:
*   [ ] **Granular Progress Reporting:** Implement more detailed progress updates from the engine to the central server (e.g., percentage complete, estimated time remaining).
*   [ ] **Comprehensive Resource Monitoring:** Beyond CPU temperature, report on RAM usage, disk I/O, and GPU utilization (if applicable).
*   [ ] **Dynamic `ffmpeg` Parameter Adjustment:** Allow the central server to dynamically adjust `ffmpeg` parameters for a job based on real-time engine load or specific job requirements.
*   [ ] **Local Caching:** Implement a local caching mechanism for frequently accessed source or transcoded files to reduce network overhead.
*   [ ] **Error Code Standardization:** Define and implement a standardized set of error codes for all components to improve debugging and error handling.
*   [ ] **Graceful Shutdown:** Implement graceful shutdown procedures for the Transcoding Engine to ensure ongoing jobs are completed or properly re-queued.
*   [ ] **Disk Space Management:** Implement logic to prevent the engine from accepting jobs if local disk space is critically low.
*   [ ] **Network Resilience:** Improve handling of transient network disconnections during file transfers or status reporting.
*   [ ] **`ffmpeg` Version Management:** Allow the central server to specify or recommend `ffmpeg` versions for engines.
*   [ ] **Plugin Architecture:** Consider a plugin architecture for `ffmpeg` filters or custom pre/post-processing scripts.
*   [ ] **Containerization Support:** Provide official Docker/Podman images for the Transcoding Engine.
*   [ ] **Hardware Acceleration Detection:** Automatically detect and report available hardware acceleration (e.g., NVENC, VAAPI, Quick Sync) to the dispatch server.
*   [ ] **Job Prioritization (Local):** Implement local prioritization of jobs if the engine receives multiple jobs simultaneously.
*   [ ] **Security Hardening:** Review and harden the engine against potential security vulnerabilities (e.g., input validation, command injection prevention).
*   [ ] **Bugfix: `ffmpeg` Process Hangs:** Implement robust mechanisms to detect and terminate hung `ffmpeg` processes.
*   [ ] **Bugfix: Incomplete File Transfers:** Ensure atomic file transfers or robust resume capabilities for large media files.
*   [ ] **Consider `libavcodec` Integration:** Investigate integrating `libavcodec` directly for finer-grained control and potential performance improvements in the long term.

### Central Server:
*   [ ] **Multiple Database Support:** Abstract the data storage layer to allow different backends (e.g., SQLite, PostgreSQL, MySQL) for improved scalability, reliability, and query capabilities.
*   [ ] **Admin Panel/Functionality:**
    *   [ ] Implement a web-based admin control panel.
    *   [ ] Implement a C++ wxWidgets native admin control panel with the following features:
        *   [ ] View and manage all jobs.
        *   [ ] View and manage all transcoding endpoints.
        *   [ ] View detailed information for selected jobs and endpoints.
        *   [ ] Perform administrative actions (e.g., pause/resume jobs, deauthenticate endpoints).
        *   [ ] Implement right-click context menus for relevant items.
        *   [ ] Implement top-level menu actions.
        *   [ ] Implement a configuration dialog.
        *   [ ] Implement an "About" dialog.
        *   [ ] Implement a "Help" dialog/section.
*   [ ] **Advanced Scheduling Algorithms:** Develop more sophisticated job scheduling that considers network latency, historical engine performance, and job deadlines.
*   [ ] **User Management and Roles:** Implement a system for user authentication and authorization, allowing different levels of access and control.
*   [ ] **Job Prioritization:** Allow users to assign priorities to their transcoding jobs, influencing their position in the processing queue.
*   [ ] **Notification System:** Integrate a notification system to alert users about job completion, failure, or significant engine status changes (e.g., email, webhooks).
*   [ ] **API Versioning:** Implement API versioning to allow for backward compatibility and smoother upgrades.
*   [ ] **Load Balancing:** Implement internal load balancing for job distribution across engines.
*   [ ] **Audit Logging:** Implement comprehensive audit logging for all critical server actions and API calls.
*   [ ] **Scalability Testing:** Conduct rigorous scalability testing to identify and address bottlenecks.
*   [ ] **Dynamic Configuration Reload:** Allow certain server configurations to be reloaded without restarting the server.
*   [ ] **Job Dependency Management:** Implement the ability to define job dependencies (e.g., Job B starts after Job A completes).
*   [ ] **Storage Tiering:** Support different storage tiers (e.g., fast SSD for active jobs, slower HDD for archives).
*   [ ] **Bugfix: Race Conditions:** Identify and fix potential race conditions in job assignment and status updates.
*   [ ] **Bugfix: Database Deadlocks:** Address any potential database deadlocks under high load.
*   [ ] **Bugfix: Orphaned Jobs:** Implement a cleanup mechanism for jobs that become orphaned due to engine failures.

### Submission Client (GUI):
*   [ ] **Drag-and-Drop File Submission:** Enable users to easily submit files by dragging and dropping them onto the application interface.
*   [ ] **Visual Progress Indicators:** Display real-time progress bars and visual cues for ongoing transcoding jobs.
*   [ ] **Real-time Data Updates:** Ensure job and engine lists update automatically without manual refreshing.
*   [ ] **Filtering and Sorting:** Provide options to filter and sort job and engine lists based on various criteria.
*   [ ] **Detailed Information Panels:** Enhance the information display for selected jobs and endpoints, showing all relevant metrics and metadata.
*   [ ] **Batch Job Submission:** Allow users to submit multiple transcoding jobs simultaneously.
*   [ ] **Pre-defined Transcoding Profiles:** Enable users to select from a set of pre-configured transcoding settings.
*   [ ] **Input Validation:** Implement robust client-side input validation for job submission parameters.
*   [ ] **Error Reporting:** Provide clear and actionable error messages to the user.
*   [ ] **Theming/Customization:** Allow users to customize the GUI theme or appearance.
*   [ ] **Internationalization (i18n):** Support multiple languages for the GUI.
*   [ ] **Offline Mode:** Allow users to prepare jobs offline and submit them when connectivity is restored.
*   [ ] **Job Template Saving:** Enable users to save frequently used job parameters as templates.
*   [ ] **Bugfix: UI Responsiveness:** Ensure the UI remains responsive during long-running operations or network delays.
*   [ ] **Bugfix: Data Desynchronization:** Address issues where client data might become out of sync with the server.

### General System Improvements:
*   [ ] **Centralized Logging and Monitoring:** Implement a unified logging and monitoring solution (e.g., integration with ELK stack, Prometheus/Grafana) for better system observability.
*   [ ] **Configuration Management:** Develop a centralized system for managing and distributing configuration settings across all components.
*   [ ] **Automated Deployment:** Implement automated deployment pipelines for all components.
*   [ ] **Performance Benchmarking:** Establish a continuous performance benchmarking framework for the entire system.
*   [ ] **Documentation Enhancement:** Continuously improve and expand all project documentation (API, user guides, architecture).
*   [ ] **Cross-Platform Compatibility:** Ensure all components are fully compatible and tested across supported operating systems (Linux, Windows, macOS, FreeBSD).
*   [ ] **Security Audits:** Conduct regular security audits and penetration testing.
*   [ ] **Disaster Recovery Plan:** Develop and test a disaster recovery plan for the central server and data.
*   [ ] **Version Control for Configurations:** Store and manage configuration files under version control.
*   [ ] **Automated Testing Integration:** Integrate all unit and integration tests into a continuous integration (CI) pipeline.
*   [ ] **Bugfix: Memory Leaks:** Conduct thorough memory profiling and fix any identified memory leaks across all C++ components.
*   [ ] **Bugfix: Resource Exhaustion:** Address potential resource exhaustion issues (e.g., file handles, network connections) under heavy load.
*   [ ] **Bugfix: Time Synchronization:** Ensure consistent time synchronization across all components to prevent job scheduling or status reporting issues.
*   [ ] **Bugfix: Data Corruption:** Implement checksums or other validation mechanisms to prevent data corruption during transfers or storage.
*   [ ] **Bugfix: Unhandled Exceptions:** Implement global exception handlers to prevent crashes and provide meaningful error logs.

## Protocol Selection

We need a reliable and efficient protocol for communication between the components. Here's a comparison of the options:

*   **HTTP REST API:**
    *   **Pros:** Simple, well-understood, easy to implement and debug. Good for the initial MVP.
    *   **Cons:** Can be verbose (text-based), may not be the most performant for streaming or real-time status updates.

*   **gRPC with Protocol Buffers:**
    *   **Pros:** High-performance, binary protocol. Well-suited for streaming. Strongly typed contracts between services.
    *   **Cons:** More complex to set up than a simple REST API.

*   **IRC/XMPP/SIP:**
    *   **Pros:** Designed for real-time communication.
    *   **Cons:** Not a natural fit for this application. These are primarily for chat and VoIP, and would require significant adaptation.

**Recommendation:**

*   **Phase 1 (MVP):** Use a **HTTP REST API** for simplicity and ease of development.
*   **Phase 2 and beyond:** Migrate to **gRPC with Protocol Buffers** for better performance and streaming capabilities.

## Component Breakdown

**1. Transcoding Engine**

*   **Language:** C++
*   **Dependencies:** `ffmpeg` (for transcoding), a C++ HTTP client/server library (e.g., Boost.Asio, cpp-httplib).
*   **Functionality:**
    *   Daemon that runs in the background.
    *   Periodically sends a heartbeat to the dispatch server with its status and storage capacity.
    *   Listens for transcoding jobs from the dispatch server.
    *   Downloads the source video.
    *   Executes `ffmpeg` with the specified parameters.
    *   Reports job completion or failure to the dispatch server.
    *   Provides the transcoded file for download.

**2. Transcoding Dispatch Server**

*   **Language:** C++
*   **Functionality:**
    *   Manages a list of transcoding engines and their status.
    *   Maintains a queue of transcoding jobs.
    *   Assigns jobs to available engines.
    *   Tracks the status of each job.
    *   Handles storage of source and transcoded videos.
    *   Provides a REST API (Phase 1) or gRPC service (Phase 2) for clients and engines.

**3. Submission Client**

*   **Language:** C++
*   **Functionality:**
    *   Command-line interface for submitting jobs.
    *   Can be used to query job status.
    *   Downloads the final transcoded video.

## Testing Strategy

Each component will have its own set of unit and integration tests.

*   **Transcoding Engine:**
    *   **Unit Tests:** Test individual functions (e.g., parsing job parameters, executing `ffmpeg`).
    *   **Integration Tests:** Test the full workflow of receiving a job, transcoding a video, and reporting completion. This will involve mocking the dispatch server.

*   **Transcoding Dispatch Server:**
    *   **Unit Tests:** Test individual API endpoints and business logic (e.g., job queueing, engine selection).
    *   **Integration Tests:** Test the interaction with the transcoding engine and submission client. This will involve running a real (or mocked) transcoding engine.

*   **Submission Client:**
    *   **Unit Tests:** Test command-line argument parsing and API client logic.
    *   **Integration Tests:** Test the full workflow of submitting a job and downloading the result against a running dispatch server.

## Protocol Documentation

The chosen protocol (initially REST, then gRPC) will be documented in detail. The documentation will include:

*   A list of all API endpoints/gRPC methods.
*   The format of all requests and responses.
*   Example usage for each endpoint/method.
*   The state machine for a transcoding job.

This documentation will be crucial for developers working on any of the three components.
ne:
*   [ ] **Granular Progress Reporting:** Implement more detailed progress updates from the engine to the central server (e.g., percentage complete, estimated time remaining).
*   [ ] **Comprehensive Resource Monitoring:** Beyond CPU temperature, report on RAM usage, disk I/O, and GPU utilization (if applicable).
*   [ ] **Dynamic `ffmpeg` Parameter Adjustment:** Allow the central server to dynamically adjust `ffmpeg` parameters for a job based on real-time engine load or specific job requirements.
*   [ ] **Local Caching:** Implement a local caching mechanism for frequently accessed source or transcoded files to reduce network overhead.

### Central Server:
*   [ ] **Multiple Database Support:** Abstract the data storage layer to allow different backends (e.g., SQLite, PostgreSQL, MySQL) for improved scalability, reliability, and query capabilities.
*   [ ] **Admin Panel/Functionality:**
    *   [ ] Implement a web-based admin control panel.
    *   [ ] Implement a C++ wxWidgets native admin control panel with the following features:
        *   [ ] View and manage all jobs.
        *   [ ] View and manage all transcoding endpoints.
        *   [ ] View detailed information for selected jobs and endpoints.
        *   [ ] Perform administrative actions (e.g., pause/resume jobs, deauthenticate endpoints).
        *   [ ] Implement right-click context menus for relevant items.
        *   [ ] Implement top-level menu actions.
        *   [ ] Implement a configuration dialog.
        *   [ ] Implement an "About" dialog.
        *   [ ] Implement a "Help" dialog/section.
*   [ ] **Advanced Scheduling Algorithms:** Develop more sophisticated job scheduling that considers network latency, historical engine performance, and job deadlines.
*   [ ] **User Management and Roles:** Implement a system for user authentication and authorization, allowing different levels of access and control.
*   [ ] **Job Prioritization:** Allow users to assign priorities to their transcoding jobs, influencing their position in the processing queue.
*   [ ] **Notification System:** Integrate a notification system to alert users about job completion, failure, or significant engine status changes (e.g., email, webhooks).

### Submission Client (GUI):
*   [ ] **Drag-and-Drop File Submission:** Enable users to easily submit files by dragging and dropping them onto the application interface.
*   [ ] **Visual Progress Indicators:** Display real-time progress bars and visual cues for ongoing transcoding jobs.
*   [ ] **Real-time Data Updates:** Ensure job and engine lists update automatically without manual refreshing.
*   [ ] **Filtering and Sorting:** Provide options to filter and sort job and engine lists based on various criteria.
*   [ ] **Detailed Information Panels:** Enhance the information display for selected jobs and endpoints, showing all relevant metrics and metadata.
*   [ ] **Batch Job Submission:** Allow users to submit multiple transcoding jobs simultaneously.
*   [ ] **Pre-defined Transcoding Profiles:** Enable users to select from a set of pre-configured transcoding settings.

### General System Improvements:
*   [ ] **Centralized Logging and Monitoring:** Implement a unified logging and monitoring solution (e.g., integration with ELK stack, Prometheus/Grafana) for better system observability.
*   [ ] **Configuration Management:** Develop a centralized system for managing and distributing configuration settings across all components.

## Protocol Selection

We need a reliable and efficient protocol for communication between the components. Here's a comparison of the options:

*   **HTTP REST API:**
    *   **Pros:** Simple, well-understood, easy to implement and debug. Good for the initial MVP.
    *   **Cons:** Can be verbose (text-based), may not be the most performant for streaming or real-time status updates.

*   **gRPC with Protocol Buffers:**
    *   **Pros:** High-performance, binary protocol. Well-suited for streaming. Strongly typed contracts between services.
    *   **Cons:** More complex to set up than a simple REST API.

*   **IRC/XMPP/SIP:**
    *   **Pros:** Designed for real-time communication.
    *   **Cons:** Not a natural fit for this application. These are primarily for chat and VoIP, and would require significant adaptation.

**Recommendation:**

*   **Phase 1 (MVP):** Use a **HTTP REST API** for simplicity and ease of development.
*   **Phase 2 and beyond:** Migrate to **gRPC with Protocol Buffers** for better performance and streaming capabilities.

## Component Breakdown

**1. Transcoding Engine**

*   **Language:** C++
*   **Dependencies:** `ffmpeg` (for transcoding), a C++ HTTP client/server library (e.g., Boost.Asio, cpp-httplib).
*   **Functionality:**
    *   Daemon that runs in the background.
    *   Periodically sends a heartbeat to the dispatch server with its status and storage capacity.
    *   Listens for transcoding jobs from the dispatch server.
    *   Downloads the source video.
    *   Executes `ffmpeg` with the specified parameters.
    *   Reports job completion or failure to the dispatch server.
    *   Provides the transcoded file for download.

**2. Transcoding Dispatch Server**

*   **Language:** C++
*   **Functionality:**
    *   Manages a list of transcoding engines and their status.
    *   Maintains a queue of transcoding jobs.
    *   Assigns jobs to available engines.
    *   Tracks the status of each job.
    *   Handles storage of source and transcoded videos.
    *   Provides a REST API (Phase 1) or gRPC service (Phase 2) for clients and engines.

**3. Submission Client**

*   **Language:** C++
*   **Functionality:**
    *   Command-line interface for submitting jobs.
    *   Can be used to query job status.
    *   Downloads the final transcoded video.

## Testing Strategy

Each component will have its own set of unit and integration tests.

*   **Transcoding Engine:**
    *   **Unit Tests:** Test individual functions (e.g., parsing job parameters, executing `ffmpeg`).
    *   **Integration Tests:** Test the full workflow of receiving a job, transcoding a video, and reporting completion. This will involve mocking the dispatch server.

*   **Transcoding Dispatch Server:**
    *   **Unit Tests:** Test individual API endpoints and business logic (e.g., job queueing, engine selection).
    *   **Integration Tests:** Test the interaction with the transcoding engine and submission client. This will involve running a real (or mocked) transcoding engine.

*   **Submission Client:**
    *   **Unit Tests:** Test command-line argument parsing and API client logic.
    *   **Integration Tests:** Test the full workflow of submitting a job and downloading the result against a running dispatch server.

## Protocol Documentation

The chosen protocol (initially REST, then gRPC) will be documented in detail. The documentation will include:

*   A list of all API endpoints/gRPC methods.
*   The format of all requests and responses.
*   Example usage for each endpoint/method.
*   The state machine for a transcoding job.

This documentation will be crucial for developers working on any of the three components.
