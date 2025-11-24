# Engine Action Plan

- [ ] **1. Replace `system()` and `popen()`**
    - [ ] Use libraries like `boost.process` or platform-native APIs for secure process management.

- [ ] **2. Parse `ffmpeg` Progress**
    - [ ] Read `stderr` output in real-time to parse progress information.

- [ ] **3. Use C++ `ffmpeg` Wrapper Library**
    - [ ] Integrate directly with `libavcodec`/`libavformat` for finer control (Long term goal).

- [ ] **4. Robust Process Management (Watchdog)**
    - [ ] Implement a watchdog to monitor and terminate hanging `ffmpeg` processes.

- [ ] **5. Refactor `main.cpp` (Classes)**
    - [ ] Break logic into `HeartbeatClient`, `JobPoller`, `Transcoder`, `ResourceManager` classes.

- [ ] **6. Use `nlohmann/json`**
    - [ ] Replace `cJSON` with `nlohmann/json` for consistency and type safety.

- [ ] **7. Use `cpr` instead of `libcurl`**
    - [ ] Use `cpr` wrapper for cleaner HTTP requests.

- [ ] **8. Structured Error Handling**
    - [ ] Replace any legacy error handling with RAII or structured approaches.

- [ ] **9. Proper SQLite Error Handling**
    - [ ] Trigger robust failure states on SQLite errors instead of just printing to `cerr`.

- [ ] **10. Prepared Statements for SQLite**
    - [ ] Use `sqlite3_prepare_v2` and bindings to prevent SQL injection.

- [ ] **11. Connection Management for SQLite**
    - [ ] Encapsulate `db` handle in a class that manages lifecycle.

- [ ] **12. Don't Detach Threads**
    - [ ] Manage thread lifecycle in the main loop and `join()` on shutdown.

- [ ] **13. Use Condition Variable for Job Polling**
    - [ ] Wait on a condition variable instead of `sleep` for job polling.

- [ ] **14. Make Heartbeat Interval Configurable**
    - [ ] Load heartbeat interval from config.

- [ ] **15. Make Benchmark Interval Configurable**
    - [ ] Load benchmark interval from config.

- [ ] **16. Hostname Configuration**
    - [ ] Allow hostname to be configured (done, verify).

- [ ] **17. Portable CPU Temp**
    - [ ] Use `libsensors` or equivalent for portable temperature reading on Linux.

- [ ] **18. Windows/macOS CPU Temp**
    - [ ] Implement temperature monitoring for Windows (WMI) and macOS (smc).

- [ ] **19. GPU Temp/Utilization**
    - [ ] Integrate `nvidia-smi` or similar for GPU monitoring.

- [ ] **20. Disk Space Monitoring**
    - [ ] Monitor available disk space and report in heartbeat.
