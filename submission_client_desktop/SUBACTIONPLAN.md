# Client Action Plan

- [ ] **1. Implement full GUI (Tabs)**
    - [ ] Create the tabbed interface for "My Jobs", "All Jobs", and "Endpoints".

- [ ] **2. Separate UI from Logic (ApiClient)**
    - [ ] Create an `ApiClient` class that handles all `cpr` calls.
    - [ ] The `MyFrame` class should only call methods on this `ApiClient` and update the UI.

- [ ] **3. Use Asynchronous Calls**
    - [ ] All calls to the `ApiClient` should be asynchronous (e.g., using `wxThread` or `std::async`).
    - [ ] Avoid freezing the GUI.

- [ ] **4. Improve Output Log**
    - [ ] Return result objects from API calls instead of redirecting `cout`.
    - [ ] Format and display results in the UI.

- [ ] **5. Implement `wxDataViewListCtrl`**
    - [ ] Use `wxDataViewListCtrl` for job and engine lists to show multiple columns.

- [ ] **6. Real-time Updates (wxTimer)**
    - [ ] Use `wxTimer` to periodically poll the server for updates.

- [ ] **7. Configuration Dialog**
    - [ ] Create a dialog to set Server URL, API Key, and Certificate Path.

- [ ] **8. Store Configuration**
    - [ ] Save configuration to a file (e.g., `~/.distconv/client.cfg`).

- [ ] **9. Drag and Drop Submission**
    - [ ] Allow users to drag video files onto the application to pre-fill the "Source URL".

- [ ] **10. Better Input Validation**
    - [ ] Add input validators (`wxValidator`) to text controls.

- [ ] **11. Job Progress Bars**
    - [ ] Include a progress bar control for jobs in the job list.

- [ ] **12. Context Menus**
    - [ ] Implement right-click context menus on lists (e.g., "Cancel Job").

- [ ] **13. Detailed Info Panel**
    - [ ] Show details in a separate panel when an item is selected.

- [ ] **14. Don't Hardcode API Key**
    - [ ] Load API Key from configuration.

- [ ] **15. Improve `cpr` Usage (Timeout)**
    - [ ] Set a timeout for all `cpr` requests.

- [ ] **16. Handle `cpr` Errors**
    - [ ] Check for transport errors in addition to HTTP status codes.

- [ ] **17. Local Job ID Storage**
    - [ ] Replace `submitted_job_ids.txt` with a small SQLite database for better metadata storage.

- [ ] **18. Refactor Console Functions**
    - [ ] Move global console functions to `ApiClient` class.

- [ ] **19. `JobOptions` Struct**
    - [ ] Pass a `JobOptions` struct to `submitJob` instead of many parameters.

- [ ] **20. CMake Improvements (FetchContent)**
    - [ ] Use `FetchContent` or package manager for `cpr` and `json` instead of subdirectories.
