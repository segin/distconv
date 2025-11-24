# Action Plan: Distributed Video Transcoding System

This document outlines the implementation plan for the first 40 architectural and design improvements identified for the project.

## I. Architecture & Design

### A. Core Architecture & Decoupling

- [ ] **1. Introduce a Message Queue**
    - [ ] Research and selection
        - [ ] Evaluate RabbitMQ (AMQP) for reliability and routing.
        - [ ] Evaluate NATS for high performance and simplicity.
        - [ ] Evaluate Redis Streams if Redis is already used for caching.
        - [ ] Select technology based on operational complexity vs. requirements.
    - [ ] Design abstraction layer
        - [ ] Define `MessageQueue` interface in C++ (Producer/Consumer).
        - [ ] Create mock implementation for testing.
    - [ ] Implementation - Dispatch Server
        - [ ] Implement `JobPublisher` to push jobs to the queue instead of assigning via HTTP response.
        - [ ] Implement `StatusSubscriber` to listen for job updates.
    - [ ] Implementation - Transcoding Engine
        - [ ] Implement `JobSubscriber` to pull/listen for jobs.
        - [ ] Implement `StatusPublisher` to push progress and completion events.
    - [ ] Robustness
        - [ ] Implement acknowledgement mechanism (ACK/NACK).
        - [ ] Design Dead Letter Queue (DLQ) handling (refer to Item 61).

- [ ] **2. Service-Oriented Architecture**
    - [ ] Analysis
        - [ ] Identify boundaries between Job Management, Engine Management, and File Serving.
    - [ ] Component extraction
        - [ ] Refactor `JobService` into a standalone module/library.
        - [ ] Refactor `EngineRegistry` into a standalone module/library.
        - [ ] Refactor `FileService` for storage operations.
    - [ ] Interface definition
        - [ ] Define strict C++ interfaces between these services.
        - [ ] Ensure no direct database access across service boundaries.

- [ ] **3. Plugin-Based Engine**
    - [ ] Architecture Design
        - [ ] Define `TranscoderPlugin` interface (capabilities, transcode, cancel).
        - [ ] Define `ResourceMonitorPlugin` interface.
    - [ ] Implementation
        - [ ] Create `FFmpegPlugin` implementing `TranscoderPlugin`.
        - [ ] Implement dynamic loading mechanism (e.g., `dlopen` or shared libraries) or compile-time registration.
    - [ ] Extension
        - [ ] Create example plugin for a different tool (e.g., Handbrake CLI wrapper or hardware-specific SDK).

- [ ] **4. Abstract Storage Layer**
    - [ ] Interface Design
        - [ ] Create `StorageProvider` interface (`Read`, `Write`, `Delete`, `List`, `GetUrl`).
    - [ ] Implementations
        - [ ] Implement `LocalStorageProvider` (current functionality).
        - [ ] Implement `S3StorageProvider` (using AWS SDK or MinIO).
    - [ ] Integration
        - [ ] Update Dispatch Server to use `StorageProvider` factory based on configuration.
        - [ ] Ensure atomic writes where possible (upload to temp, then rename/commit).

- [ ] **5. State Machine Implementation**
    - [ ] Design
        - [ ] Map all valid job states: `Pending`, `Assigned`, `Transcoding`, `Completing`, `Completed`, `Failed`, `Retrying`, `Cancelled`.
        - [ ] Define transition table (e.g., `Pending` -> `Assigned`, `Assigned` -> `Failed`).
    - [ ] Implementation
        - [ ] Create `JobStateMachine` class.
        - [ ] Implement strict transition enforcement (throw error on invalid transition).
        - [ ] Add side-effect hooks (e.g., "OnEnterFailed" -> increment retry count).
    - [ ] Persistence
        - [ ] Ensure state transitions are persisted to SQLite immediately.

- [ ] **6. Adopt gRPC**
    - [ ] Definition
        - [ ] Define `.proto` files for Job and Engine services.
        - [ ] Define messages for `JobRequest`, `JobStatus`, `Heartbeat`, etc.
    - [ ] Build System integration
        - [ ] Add `protoc` and gRPC C++ plugin to CMake.
    - [ ] Implementation
        - [ ] Generate C++ stubs.
        - [ ] Replace internal REST client/server logic with gRPC calls.
    - [ ] Migration
        - [ ] Maintain dual-stack (REST + gRPC) during transition if necessary.

- [ ] **7. Configuration Service**
    - [ ] Requirement gathering
        - [ ] Identify all configuration parameters (DB paths, timeouts, thresholds).
    - [ ] Design
        - [ ] Choose backend: Centralized file, Database, or dedicated service (Consul/etcd).
    - [ ] Implementation
        - [ ] Create `ConfigManager` singleton/dependency.
        - [ ] Implement hot-reloading of configuration where safe.
        - [ ] Remove all hardcoded constants and command-line parsing logic for deep config.

- [ ] **8. Separate API Gateway**
    - [ ] Design
        - [ ] Design architecture where Dispatch Server is internal-only.
        - [ ] Select Gateway technology (Nginx, Traefik, or custom C++ layer).
    - [ ] Implementation
        - [ ] Configure Gateway for Authentication termination.
        - [ ] Configure Rate Limiting at the Gateway.
        - [ ] Route `/api/v1/*` to Dispatch Server.

- [ ] **9. Event-Driven Architecture**
    - [ ] Event Definitions
        - [ ] Define schema for events: `JobCreated`, `JobAssigned`, `TranscodingStarted`, `ProgressUpdated`, `JobFinished`.
    - [ ] Infrastructure
        - [ ] Set up an internal Event Bus (in-memory or over MQ).
    - [ ] Refactoring
        - [ ] Change `JobScheduler` to subscribe to `JobCreated` and `EngineHeartbeat` events.
        - [ ] Change `NotificationService` (Webhooks) to subscribe to status change events.

- [ ] **10. Idempotency in API**
    - [ ] Design
        - [ ] Identify non-idempotent operations (Submission, Retry).
        - [ ] Add `Idempotency-Key` header requirement for sensitive POST requests.
    - [ ] Implementation
        - [ ] Store processed keys in Redis or SQLite with expiration.
        - [ ] Middleware to check incoming keys and return cached response if already processed.

### B. Scalability & Performance

- [ ] **11. Horizontal Scaling for Dispatch Server**
    - [ ] State Externalization
        - [ ] Move any in-memory state (local queues) to Redis or shared DB.
    - [ ] Locking
        - [ ] Implement distributed locking (e.g., Redis Lock) for job assignment to prevent double-assignment.
    - [ ] Load Balancing
        - [ ] Configure Nginx/HAProxy to round-robin requests to server instances.

- [ ] **12. Database Read Replicas**
    - [ ] Infrastructure
        - [ ] Set up Master-Slave replication for the database (PostgreSQL/MySQL if migrated, or rqlite).
    - [ ] Code Changes
        - [ ] Update `Repository` layer to separate `ReadOnlyConnection` and `ReadWriteConnection`.
        - [ ] Route `GET` requests to Read Replicas.

- [ ] **13. Connection Pooling**
    - [ ] Implementation
        - [ ] Implement or integrate a generic object pool for Database connections.
        - [ ] Configure min/max pool size and idle timeouts.
    - [ ] Integration
        - [ ] Replace single `sqlite3*` handle with `ConnectionPool::acquire()`.

- [ ] **14. Caching Layer**
    - [ ] Strategy
        - [ ] Identify cacheable entities (Engine details, Job status, Config).
    - [ ] Implementation
        - [ ] Integrate Redis client (`redis-plus-plus` or similar).
        - [ ] Implement `Write-Through` or `Cache-Aside` pattern in Repositories.
    - [ ] Invalidation
        - [ ] Ensure cache invalidation on state changes.

- [ ] **15. Content Delivery Network (CDN)**
    - [ ] Integration
        - [ ] Update `StorageProvider` (S3) to generate public URLs pointing to the CDN domain instead of direct bucket URLs.
    - [ ] Configuration
        - [ ] Add CDN base URL configuration.

- [ ] **16. Sharding the Jobs Database**
    - [ ] Strategy
        - [ ] Choose sharding key (e.g., `User_ID`, `Time_Range`, or Hash of `Job_ID`).
    - [ ] Implementation
        - [ ] Implement `ShardRouter` to determine which DB instance to query.
        - [ ] Handle cross-shard queries (aggregation) if necessary (or avoid them).

- [ ] **17. Use `io_uring`**
    - [ ] Investigation
        - [ ] Check kernel version compatibility on target deployment.
    - [ ] Implementation
        - [ ] Replace `epoll`/`select` in the networking layer with `liburing`.
        - [ ] Implement async file I/O using `io_uring` for video upload/download.

- [ ] **18. Offload SSL/TLS Termination**
    - [ ] Configuration
        - [ ] Disable SSL/TLS code in Dispatch Server (or make optional).
        - [ ] Configure Nginx sidecar or ingress controller to handle certificates.
        - [ ] Ensure communication between LB and Server is trusted (or use mTLS).

- [ ] **19. Optimize JSON Parsing**
    - [ ] Benchmarking
        - [ ] Profile current `nlohmann/json` usage under load.
    - [ ] Replacement
        - [ ] Evaluate `simdjson` or `RapidJSON` for critical paths.
        - [ ] Refactor serialization code to use the new library where performance is critical.

- [ ] **20. Asynchronous I/O in C++ Components**
    - [ ] Framework adoption
        - [ ] Evaluate Boost.Asio or Seastar.
    - [ ] Refactoring
        - [ ] Rewrite request handlers to be non-blocking.
        - [ ] Use coroutines (C++20) for cleaner async code structure.

### C. Transcoding Engine Design

- [ ] **21. Isolate `ffmpeg` Process**
    - [ ] Technology Selection
        - [ ] Choose isolation level: Docker API, `systemd-nspawn`, or `bubblewrap`.
    - [ ] Implementation
        - [ ] Wrap `ffmpeg` execution to run inside the container/sandbox.
        - [ ] Map necessary volumes (input/output) into the sandbox.

- [ ] **22. Local Job Queue Persistence**
    - [ ] Database Schema
        - [ ] Ensure SQLite schema in engine supports `queue_order`, `retry_count`.
    - [ ] Logic
        - [ ] On startup, load pending jobs from SQLite.
        - [ ] On job arrival, write to SQLite before acknowledging receipt.
        - [ ] On completion, remove from SQLite.

- [ ] **23. Engine Self-Registration**
    - [ ] Protocol
        - [ ] Define `POST /engines/register` payload (Capabilities, Hostname, IP).
    - [ ] Server Logic
        - [ ] Generate temporary auth token or auto-approve based on policy.
    - [ ] Engine Logic
        - [ ] Call register endpoint on startup.
        - [ ] Store assigned `engine_id` and credentials locally.

- [ ] **24. Graceful Shutdown for Engines**
    - [ ] Signal Handling
        - [ ] Catch `SIGTERM` and `SIGINT`.
    - [ ] State Management
        - [ ] Set state to `Draining`.
        - [ ] Stop accepting new jobs (reject assignment requests).
        - [ ] Wait for current transcoding to finish.
        - [ ] Send `EngineOffline` event to server.
        - [ ] Exit.

- [ ] **25. Engine Self-Update Mechanism**
    - [ ] Design
        - [ ] Server endpoint to check latest version/binary URL.
    - [ ] Implementation
        - [ ] Engine checks version periodically.
        - [ ] If new version:
            - [ ] Download new binary.
            - [ ] Verify signature/hash.
            - [ ] Spawn new process.
            - [ ] Gracefully shutdown current process.

- [ ] **26. Backpressure Handling**
    - [ ] Metrics
        - [ ] Monitor CPU Load, Memory Usage, Disk I/O Wait.
    - [ ] Logic
        - [ ] Define thresholds (e.g., >90% CPU).
        - [ ] If thresholds exceeded, reject new job assignments or report `Busy` status in heartbeat even if queue is empty.

- [ ] **27. Off-Heap Storage**
    - [ ] Implementation
        - [ ] Use `mmap` for reading large input files during upload/processing if applicable.
        - [ ] Ensure memory limits are enforced on buffers.

- [ ] **28. Hardware Affinity**
    - [ ] Configuration
        - [ ] Allow tagging engine with specific hardware IDs (e.g., `GPU:0`, `CPU_Set:1-4`).
    - [ ] Execution
        - [ ] Apply `taskset` (Linux) or CUDA_VISIBLE_DEVICES when launching `ffmpeg`.

- [ ] **29. Dynamic Capability Reporting**
    - [ ] Monitoring
        - [ ] Periodically run capability checks (e.g., check if GPU driver is still loaded).
    - [ ] Reporting
        - [ ] Update `supported_codecs` and `hardware_acceleration` flags in the Heartbeat payload if changed.

- [ ] **30. Streaming vs. File Transfer Logic**
    - [ ] Decision Matrix
        - [ ] Inputs: Network Speed, File Size, Engine Disk Space.
        - [ ] Logic: If (Size > DiskFree) OR (Network < Threshold), use Streaming.
    - [ ] Implementation
        - [ ] Implement Streaming Transcoding path (Pipe input from HTTP -> FFmpeg -> HTTP upload).

### D. Dispatch Server Design

- [ ] **31. Separate Job Scheduler**
    - [ ] Architecture
        - [ ] Decouple `Scheduler` from `JobController`.
        - [ ] `Scheduler` runs in its own thread/service loop.
    - [ ] Logic
        - [ ] Pull pending jobs.
        - [ ] Match with available engines.
        - [ ] Assign.

- [ ] **32. Fairness in Scheduling**
    - [ ] Algorithm
        - [ ] Implement Weighted Fair Queuing (WFQ) or Round Robin per User/Tenant.
    - [ ] Implementation
        - [ ] Track active jobs per user.
        - [ ] Deprioritize users exceeding their share.

- [ ] **33. Job-Engine Affinity**
    - [ ] Data Model
        - [ ] Add `required_capabilities` to Job.
        - [ ] Add `tags` to Engine.
    - [ ] Matching Logic
        - [ ] Filter engines that possess all `required_capabilities` of the job.

- [ ] **34. Time-Based Job Expiration** (Note: Partially implemented)
    - [ ] Implementation
        - [ ] Background thread checks `created_at` timestamp of `pending` jobs.
        - [ ] If `now - created_at > ttl`, transition to `Failed` with `reason="Expired"`.
    - [ ] Configuration
        - [ ] Make TTL configurable globally and per-job.

- [ ] **35. Webhook Support**
    - [ ] Data Model
        - [ ] Add `webhook_url` to Job submission payload.
    - [ ] Implementation
        - [ ] On job completion/failure, trigger HTTP POST to `webhook_url`.
        - [ ] Include JSON payload with job status and output URL.
        - [ ] Implement retries for failed webhook delivery.

- [ ] **36. API Versioning in URL** (Note: Partially implemented)
    - [ ] Routing
        - [ ] Ensure all routes are prefixed with `/api/v1/`.
    - [ ] Version Handler
        - [ ] Prepare structure for `/api/v2/` future support.

- [ ] **37. Standardized Error Responses**
    - [ ] Design
        - [ ] Define standard JSON structure: `{"error": {"code": "ERR_CODE", "message": "Human readable", "details": {...}}}`.
    - [ ] Implementation
        - [ ] Create `APIException` class hierarchy.
        - [ ] Create global exception handler to catch and format JSON response.

- [ ] **38. Pagination for List Endpoints**
    - [ ] API Design
        - [ ] Add query params: `limit`, `offset` (or `cursor`).
    - [ ] Database
        - [ ] Implement `LIMIT` and `OFFSET` in SQL queries.
    - [ ] Response
        - [ ] Return metadata: `total_count`, `next_page_url`.

- [ ] **39. Support for `HEAD` Requests**
    - [ ] Implementation
        - [ ] Register `HEAD` handlers for `/jobs/{id}`, `/engines/{id}`.
        - [ ] Perform DB lookup but return no body.
        - [ ] Set `Content-Length` and `Last-Modified` headers.

- [ ] **40. ETag Support**
    - [ ] Logic
        - [ ] Calculate hash of resource state (e.g., job status + update time).
    - [ ] Implementation
        - [ ] Send `ETag` header in responses.
        - [ ] Check `If-None-Match` header in requests.
        - [ ] Return `304 Not Modified` if matches.
