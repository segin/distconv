# Architecture Overview

This document is a living reference designed to give agents and contributors a rapid, comprehensive understanding of the DistConv codebase architecture, enabling efficient navigation and effective contribution from day one. Update this document as the codebase evolves.

## 1. Project Structure

```
distconv/
├── dispatch_server_cpp/          # Central dispatch server (C++17)
│   ├── dispatch_server_core.cpp/h    # Core HTTP server, API handlers, state machine
│   ├── job_scheduler.cpp/h           # Job scheduling and assignment logic
│   ├── repositories.cpp/h            # SQLite repository layer (jobs + engines)
│   ├── job_handlers.cpp/h            # POST /jobs, GET /jobs handlers
│   ├── engine_handlers.cpp/h         # POST /engines/heartbeat, GET /engines
│   ├── job_action_handlers.cpp/h     # /complete, /fail, /cancel endpoints
│   ├── assignment_handler.cpp/h      # /assign_job endpoint
│   ├── storage_pool_handler.cpp/h    # Storage pool management
│   ├── job_update_handler.cpp/h      # /jobs/:id/update endpoint
│   ├── message_queue.h               # IMessageQueue abstract interface
│   ├── memory_message_queue.h        # In-memory (sliding window) queue impl
│   ├── redis_message_queue.cpp/h     # Redis Streams queue impl
│   ├── server_config.cpp/h           # Config loading (CLI + file)
│   ├── app_main_modern.cpp           # Entry point (modern build)
│   ├── httplib.h                     # Embedded cpp-httplib (multi-threaded HTTP)
│   └── tests/                        # 150+ GTest unit & integration tests
│
├── transcoding_engine/           # Worker transcoding engine (C++17)
│   ├── src/
│   │   ├── core/
│   │   │   ├── transcoding_engine.cpp/h      # Main engine orchestrator
│   │   ├── interfaces/                        # Abstract interfaces (DI contracts)
│   │   │   ├── database_interface.h
│   │   │   ├── http_client_interface.h
│   │   │   ├── subprocess_interface.h
│   │   │   └── message_queue.h
│   │   ├── implementations/                   # Production implementations
│   │   │   ├── cpr_http_client.cpp/h          # HTTP via libcpr
│   │   │   ├── sqlite_database.cpp/h          # SQLite local job DB
│   │   │   ├── secure_subprocess.cpp/h        # Sandboxed FFmpeg subprocess
│   │   │   └── redis_message_queue.cpp/h      # Redis consumer group impl
│   │   └── mocks/                             # GMock implementations for tests
│   ├── transcoding_engine_core.cpp/h          # Legacy core (pre-modern)
│   ├── backoff_timer.h                        # Exponential backoff utility
│   ├── app_main_modern.cpp                    # Entry point (modern build)
│   ├── tests_modern/                          # 37 GTest tests (100% pass rate)
│   └── deployment/                            # Docker, SystemD service files
│
├── submission_client_desktop/    # Qt desktop client (C++17 + Qt5/6)
│   ├── submission_client_core.cpp/h   # API client + job submission logic
│   ├── app_main.cpp                   # Entry point
│   └── tests/                         # Client unit tests
│
├── docs/                         # Protocol & API documentation
│   ├── endpoint_protocol.md          # Dispatch server REST API spec
│   ├── submissions_protocol.md       # Submission client protocol spec
│   ├── architecture_and_components.md
│   └── user_guide.md
│
├── deployment/                   # Infrastructure & deployment
│   ├── install.sh                    # Automated one-shot installer
│   ├── uninstall.sh
│   ├── distconv-dispatch.service     # SystemD unit: dispatch server
│   ├── distconv-transcoding-engine.service  # SystemD unit: transcoding engine
│   ├── docker/                       # Dockerfiles + docker-compose
│   └── k8s/                          # Kubernetes manifests
│
├── third_party/                  # Vendored dependencies
├── config/                       # Runtime config templates
├── local/                        # Local scratch (not committed, in .gitignore)
├── .github/                      # GitHub Actions CI/CD workflows
├── .gitignore
├── README.md
└── ARCHITECTURE.md               # This document
```

## 2. High-Level System Diagram

```
                         ┌─────────────────────────────┐
                         │      Dispatch Server         │
                         │       (C++17 httplib)        │
   [Submission Client] ──►  REST API (:8080)            │
   [curl / any HTTP]  ──►  Job Scheduler                ◄── [Engine Heartbeats]
                         │  SQLite Repositories         │
                         │  Async State Persistence     │
                         └────────────┬────────────────-┘
                                      │ Redis Streams
                                      │ (Consumer Groups)
                         ┌────────────▼────────────────-┐
                         │     Transcoding Engine(s)    │
                         │       (C++17, N workers)     │
                         │  Poll jobs from Redis        │
                         │  Spawn FFmpeg subprocess     │
                         │  Report status via HTTP      │
                         └─────────────────────────────-┘
                                      │
                              [Shared Storage / URLs]
```

## 3. Core Components

### 3.1. Submission Client

**Name:** DistConv Desktop Client  
**Description:** Qt-based desktop GUI for end-users to submit video files for transcoding, configure codec/quality parameters, and monitor job status with real-time progress updates. Communicates exclusively with the Dispatch Server via its REST API.  
**Technologies:** C++17, Qt 5/6, libcurl (or Qt Network)  
**Deployment:** User workstation (Linux, Windows, macOS)

### 3.2. Backend Services

#### 3.2.1. Dispatch Server

**Name:** `dispatch_server_cpp`  
**Description:** The central coordinator and "brain" of the system. Exposes a multi-threaded REST API for job submission, status queries, and engine management. Maintains authoritative job and engine state in SQLite with asynchronous persistence. Assigns jobs to engines based on codec capability matching. Publishes jobs to Redis Streams and tracks engine heartbeats to detect failures.  
**Technologies:** C++17, cpp-httplib (embedded), SQLite3, redis-plus-plus (Redis Streams), nlohmann/json, GTest/GMock  
**Deployment:** Single instance (Linux bare metal or Docker); StatefulSet if Kubernetes

#### 3.2.2. Transcoding Engine

**Name:** `transcoding_engine`  
**Description:** Stateless worker node that consumes jobs from the Redis Stream consumer group, spawns FFmpeg as a sandboxed subprocess (via `SecureSubprocess`), and reports completion or failure back to the Dispatch Server via HTTP. Multiple engine instances can run concurrently on different machines for horizontal scaling. Maintains a local SQLite database for in-flight job tracking across restarts.  
**Technologies:** C++17, libcpr (HTTP), Redis Streams (redis-plus-plus), SQLite3, FFmpeg (subprocess), GTest/GMock  
**Deployment:** One process per worker node; horizontally scalable via Docker / SystemD / Kubernetes

## 4. Data Stores

### 4.1. Primary Job & Engine Database

**Name:** Dispatch Server SQLite DB  
**Type:** SQLite3 (WAL mode, prepared statements)  
**Purpose:** Authoritative persistent store for all jobs and registered engines. Survives server restarts; state is loaded on startup.  
**Key Tables:** `jobs`, `engines`

### 4.2. Local Engine Job Database

**Name:** Transcoding Engine SQLite DB  
**Type:** SQLite3  
**Purpose:** Per-engine local cache of in-progress and recently-completed jobs. Allows the engine to resume work after a restart without re-querying the dispatch server.  
**Key Tables:** `local_jobs`

### 4.3. Message Queue

**Name:** Redis Streams  
**Type:** Redis (Consumer Groups)  
**Purpose:** Durable, at-least-once delivery of job assignments from the Dispatch Server to Transcoding Engines. Consumer Groups guarantee exactly-one assignment semantics across multiple engine replicas. An in-memory implementation (`MemoryMessageQueue`) is available for single-node and test deployments.

## 5. External Integrations / APIs

| Integration | Purpose | Method |
|---|---|---|
| **FFmpeg** | Video transcoding (H.264, H.265, VP8, VP9, AV1, etc.) and hardware-accelerated encoding (NVENC, VAAPI) | Subprocess (`SecureSubprocess`) |
| **Redis** | Job queue (Streams + Consumer Groups) | redis-plus-plus C++ client |
| **libcpr / libcurl** | HTTP communication between engine and dispatch server | C++ library |
| **Source/Output URLs** | Input video files fetched and output files uploaded via HTTP URLs | libcurl/cpr |

## 6. Deployment & Infrastructure

**Deployment targets:** Linux bare metal, Docker, Kubernetes  
**Key services:**
- SystemD units: `distconv-dispatch.service`, `distconv-transcoding-engine.service`  
- Docker: `deployment/docker/` (individual images + docker-compose)  
- Kubernetes: `deployment/k8s/` (manifest templates)  
- Automated installer: `deployment/install.sh` (builds from source, creates system user, configures firewall and log rotation)

**CI/CD Pipeline:** GitHub Actions (`.github/`)  
**Monitoring & Logging:** journald (SystemD), Fluentd-compatible JSON log output, `/health` endpoint, Prometheus-compatible `/metrics` endpoint (optional)

## 7. Security Considerations

**Authentication:** `X-API-Key` header required on all API endpoints. Key configured via `--api-key` CLI flag or `DISTCONV_API_KEY` environment variable.  
**Authorization:** Single shared API key (all-or-nothing); no per-role ACL currently.  
**Data Encryption:** TLS in transit supported via OpenSSL/libcurl; at-rest encryption not applied (SQLite files rely on OS-level permissions).  
**Key Security Practices:**
- `SecureSubprocess` isolates FFmpeg execution to prevent worker process contamination
- SQL injection prevention via prepared statements in all SQLite queries
- Input validation on all API request bodies

## 8. Development & Testing

**Local Setup:**
```bash
git clone --recursive https://github.com/segin/distconv.git
cd distconv

# Dispatch server
cd dispatch_server_cpp && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j$(nproc)
./dispatch_server_tests           # 150+ tests

# Transcoding engine (modern build)
cd ../../transcoding_engine && mkdir build_modern && cd build_modern
cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j$(nproc)
./transcoding_engine_modern_tests # 37/37 tests
```

**Testing Frameworks:** GTest, GMock  
**Code Quality Tools:** clang-format (`.clang-format` in root and `dispatch_server_cpp/`), clang-tidy, cppcheck, valgrind  
**Key Test Categories:**
- Dispatch server: API endpoints, thread safety, state persistence, scheduling logic, engine heartbeat management
- Transcoding engine: dependency injection via mocks, HTTP client, SQLite ops, subprocess execution, error recovery, backoff behaviour

## 9. Future Considerations / Roadmap

- Replace JSON file–based state persistence with full SQLite for dispatch server (in progress via `repositories.cpp`)
- Add per-engine RBAC / scoped API keys for multi-tenant deployments
- Prometheus metrics exporter (endpoint stub exists)
- gRPC alternative to HTTP for engine↔dispatcher communication
- GUI progress estimation using FFmpeg duration parsing

## 10. Project Identification

**Project Name:** DistConv  
**Repository URL:** https://github.com/segin/distconv  
**Primary Contact/Team:** segin  
**Date of Last Update:** 2026-02-26

## 11. Glossary / Acronyms

| Term | Definition |
|---|---|
| **Dispatch Server** | Central coordinator; manages job queue, engine registry, and REST API |
| **Transcoding Engine** | Worker node; pulls jobs from Redis, runs FFmpeg, reports results |
| **Consumer Group** | Redis mechanism ensuring each job is delivered to exactly one engine |
| **SecureSubprocess** | C++ abstraction that spawns FFmpeg as an isolated child process |
| **WAL** | Write-Ahead Logging — SQLite journal mode enabling concurrent reads during writes |
| **NVENC / VAAPI** | GPU-accelerated video encoding APIs (NVIDIA / Linux VA-API) |
| **DI** | Dependency Injection — used throughout the transcoding engine for testability |
| **RAII** | Resource Acquisition Is Initialization — C++ idiom used for all resource lifecycle management |
