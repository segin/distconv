# Server Action Plan

- [ ] **1. Eliminate Global State**
    - [ ] Refactor to remove global variables like `API_KEY`, `jobs_db`, and `engines_db`.
    - [ ] Pass them as dependencies (e.g., in a context/application object).

- [ ] **2. Better JSON Error Handling**
    - [ ] Improve `try-catch` blocks in request handlers.
    - [ ] Provide specific error messages to the client about invalid JSON payloads.

- [ ] **3. Validate `output_url`**
    - [ ] Check `output_url` provided by the engine for validity (format, protocol).

- [ ] **4. Handle String Conversion Exceptions**
    - [ ] Add error handling for `std::to_string` and `stod` conversions.

- [ ] **5. Validate `max_retries`**
    - [ ] Ensure `max_retries` is a non-negative integer.

- [ ] **6. Handle Corrupt State File**
    - [ ] Implement logic to handle corrupt `dispatch_server_state.json`.
    - [ ] Exit or restore from backup instead of starting with empty state.

- [ ] **7. Heartbeat Timestamp Update**
    - [ ] Update `last_seen` timestamp for the engine on every heartbeat.

- [ ] **8. Stale Engine Cleanup**
    - [ ] Implement background task to remove silent engines.
    - [ ] Make timeout configurable.

- [ ] **9. Optimize Engine Selection**
    - [ ] optimize `assign_job` logic to avoid sorting engines on every call.
    - [ ] Use maintained sorted lists or efficient data structures.

- [ ] **10. Sort Engine List**
    - [ ] Implement sorting for `/engines/` endpoint (e.g., by hostname).

- [ ] **11. Track `ffmpeg` Versions**
    - [ ] Store `ffmpeg` version from engine heartbeat.
    - [ ] Filter engines based on version requirements (optional future step).

- [ ] **12. Engine Maintenance Mode**
    - [ ] Add API endpoint to toggle maintenance mode for an engine.
    - [ ] Prevent assignment to engines in maintenance mode.

- [ ] **13. Show Current Job in Engine List**
    - [ ] Include `current_job_id` in `/engines/` response.

- [ ] **14. Historical Performance Data**
    - [ ] Store historical performance metrics for engines.

- [ ] **15. Job Assignment History**
    - [ ] Track history of all engines a job was assigned to (for retries/auditing).

- [ ] **16. Replace `std::endl`**
    - [ ] Replace `std::endl` with `\n` in logging/output.

- [ ] **17. Sort Job List**
    - [ ] Ensure `/jobs/` endpoint returns jobs sorted by submission time.

- [ ] **18. Add Filtering to List Endpoints**
    - [ ] Implement query parameters for filtering (e.g., `?status=pending`).

- [ ] **19. Add Thread Pool**
    - [ ] Implement/Integrate a thread pool for background tasks.
    - [ ] Remove usage of detached threads for periodic tasks.

- [ ] **20. Optimize Pending Job Lookup**
    - [ ] Implement a separate queue/index for pending job IDs to avoid O(N) scan.
