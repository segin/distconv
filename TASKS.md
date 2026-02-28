# Project Tasks (Consolidated from SUGGESTIONS.md)

This list contains actionable tasks to improve the Distributed Transcoding Project, categorized for focus.

## I. Architecture & Core Design
- [ ] **Standardize C++ Features:** Use C++20/23 features (Concepts, Coroutines, Modules).
- [ ] **Refactor for SRP:** Break down large files like `dispatch_server_cpp/main.cpp` into smaller, single-responsibility units.
- [ ] **Modernize Memory Management:** Ensure `std::unique_ptr` and `std::shared_ptr` are used consistently; eliminate raw `new`/`delete`.
- [ ] **Portability with `std::filesystem`:** Replace all raw string path manipulations with `std::filesystem`.
- [ ] **Decouple Components:** Formalize the API-first design; separate public (client) and private (engine) APIs.
- [ ] **Protocol Versioning:** Implement a formal handshake/heartbeat that exchanges version information.
- [ ] **Structured Logging:** Replace `std::cout`/`std::cerr` with a library like `spdlog` or `glog` with log rotation.
- [ ] **Common Abstraction Layer:** Create abstractions for threading, networking, and filesystem to improve cross-platform support.
- [ ] **Atomic File Operations:** Ensure all state and video writes use temporary files and atomic renames.
- [ ] **Component Health Checks:** Add `/health` and `/metrics` (Prometheus) endpoints to all services.

## II. Security & Hardening
- [ ] **Revamp Authentication:**
    - [ ] Replace global `API_KEY` with per-client/per-engine keys.
    - [ ] Implement JWTs for stateless sessions.
    - [ ] Support API key revocation without restarts.
- [ ] **Secure ffmpeg Execution:**
    - [ ] **CRITICAL:** Replace `system()` and `popen()` with a robust subprocess library.
    - [ ] Strict input validation/allowlisting for all `ffmpeg` arguments.
    - [ ] Disable unnecessary `ffmpeg` network features by default.
- [ ] **Network Security:**
    - [ ] Enforce TLS 1.2/1.3 only; disable insecure ciphers.
    - [ ] Implement mTLS for engine-to-server communication.
    - [ ] Configure CORS with a strict allowlist.
    - [ ] Add security headers (CSP, HSTS, etc.) to all HTTP responses.
- [ ] **Data Protection:**
    - [ ] Use constant-time comparisons for security tokens.
    - [ ] Sanitize all user input to prevent SSRF, Log Injection, and Path Traversal.
    - [ ] Use signed, short-lived URLs for video file access.
- [ ] **Dependency Safety:** Automate CVE scanning for all libraries (`libcurl`, `OpenSSL`, `sqlite3`, etc.).

## III. Transcoding Engine (`transcoding_engine`)
- [ ] **Process Management:**
    - [ ] Parse `ffmpeg` output for real-time progress reporting.
    - [ ] Use a C++ wrapper library for `ffmpeg` interactions.
    - [ ] Implement job cancellation and "draining" mode for graceful shutdowns.
- [ ] **Resource Monitoring:**
    - [ ] Monitor CPU/GPU temperature and load.
    - [ ] Monitor disk space capacity and usage (don't hardcode).
    - [ ] Implement resource limits (ulimit/cgroups).
- [ ] **Job Handling:**
    - [ ] Replace job polling with WebSockets or long polling.
    - [ ] Ensure persistence of Engine ID across restarts.
    - [ ] Implement exponential backoff for server connection retries.
    - [ ] Implement robust file cleanup using `scope_exit` or RAII.
- [ ] **Wait-free Networking:** Reuse CURL handles; handle HTTP response codes properly.
- [ ] **Streaming Support:** Implement real-time streaming by piping `ffmpeg` output to HTTP POST.

## IV. Submission Client (`submission_client_desktop`)
- [ ] **UI Modernization (wxWidgets):**
    - [ ] Decouple UI logic from API calls (use async calls).
    - [ ] Add tooltips, icons, and better sizers for responsive layout.
    - [ ] Implement `wxDataViewListCtrl` for the job list.
    - [ ] Add progress bars and context menus for jobs.
- [ ] **Features:**
    - [ ] Implement a local job cache (SQLite) for offline experience.
    - [ ] Support batch submission and drag-and-drop.
    - [ ] Add a CLI client for automation.
    - [ ] Add a configuration dialog for API keys, server profiles, and CA certificates.
    - [ ] Support downloading completed files directly from the client.
- [ ] **UX Improvements:**
    - [ ] System tray integration with notifications.
    - [ ] Remember window size/position between sessions.
    - [ ] Automatic reconnection to the last used server.

## V. Infrastructure & Quality Assurance
- [ ] **Build System:**
    - [ ] Standardize `CMakeLists.txt` (use `target_include_directories`, `FetchContent`).
    - [ ] Automate release packaging with CPack.
    - [ ] Centralize versioning (e.g., `version.h.in`).
- [ ] **Testing:**
    - [ ] Comprehensive unit tests (GTest) for all logic.
    - [ ] Integration tests covering the full end-to-end job flow.
    - [ ] Enable AddressSanitizer and UndefinedBehaviorSanitizer in CI.
    - [ ] Implement fuzz testing for API endpoints.
- [ ] **CI/CD:** Set up GitHub Actions or similar for automated builds/tests/linting.
- [ ] **Git Hygiene:** Clean up `.gitignore` to include all build artifacts and CMake user files.
- [ ] **Documentation:**
    - [ ] Formal OpenAPI 3.0 spec for the protocol.
    - [ ] Doxygen for code documentation.
    - [ ] Add `CONTRIBUTING.md` and `LICENSE` files.
- [ ] **DevOps:** Provide `Dockerfile`s and `docker-compose.yml` for easy deployment.

## VI. General Refactoring
- [ ] **Standardize JSON:** Use `nlohmann/json` consistently; remove `cJSON`.
- [ ] **Standardize Libs:** Consolidate on `cpr` instead of raw `libcurl` where possible.
- [ ] **SQLite Best Practices:** Use prepared statements and handle errors/locks (WAL mode) consistently.
- [ ] **Code Style:** Enforce a consistent style with `.clang-format` and `.clang-tidy`.
- [ ] **Deduplicate Code:** Remove redundant functions (e.g., duplicated `callback` or `init_sqlite` in engine).
