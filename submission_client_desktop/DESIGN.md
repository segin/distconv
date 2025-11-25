# DISTCONV SUBMISSION CLIENT DESIGN

## Overview

The DistConv Submission Client Design provides a robust, responsive desktop interface for the DistConv distributed transcoding system. The design implements a "T-split" layout to facilitate the simultaneous monitoring of global system status, specific transcoding endpoints, and individual job queues.

The implementation centers on a C++ application using the wxWidgets toolkit for cross-platform UI rendering and `libcpr` for RESTful communication with the Dispatch Server.

The design emphasizes:
- **Responsiveness**: Keeping the UI fluid during network operations via threaded API calls.
- **Granular Control**: Enabling drag-and-drop resource reallocation.
- **Observability**: Providing real-time visibility into the state of every compute node.
- **Reliability**: ensuring persistent configuration and robust error handling.

## Architecture

### Core Components

```
SubmissionClient App
├── MainFrame (wxFrame)
│   ├── TopPane (ServerStatusPanel)
│   ├── BottomSplitter (wxSplitterWindow)
│   │   ├── LeftPane (EndpointListPanel)
│   │   └── RightPane (JobQueuePanel)
│   └── MenuBar / StatusBar
├── Network Layer
│   ├── ApiClient (CPR Wrapper)
│   └── NetworkWorker (Background Thread)
├── Data Models
│   ├── Endpoint
│   ├── Job
│   └── ServerMetrics
└── Managers
    ├── SettingsManager
    └── NotificationManager
```

### Design Rationale

**T-Split Layout**: The vertical split separating global status (Top) from detailed management (Bottom), with a further horizontal split for the Master-Detail view of Endpoints (Left) and Jobs (Right), maximizes information density. This allows operators to maintain context while drilling down into specific problems.

**wxWidgets Toolkit**: Chosen for its native look and feel on Windows, Linux, and macOS, ensuring the application feels at home in any environment. Its event-driven architecture maps well to the asynchronous nature of network updates.

**Threaded Networking**: All API calls (CPR) are offloaded to background threads or handlers to prevent blocking the main UI thread. This ensures that a slow network connection never freezes the application window.

**Drag-and-Drop Reallocation**: Implementing native drag-and-drop between the Job Queue and Endpoint List provides an intuitive, physical metaphor for moving workloads ("picking up" a job and "placing" it on a machine).

## Components and Interfaces

### MainFrame Class

```cpp
class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& title);

    // Event Handlers
    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnSettings(wxCommandEvent& event);

    // UI Update Slots
    void UpdateServerStatus(const ServerMetrics& metrics);
    void UpdateEndpointList(const std::vector<Endpoint>& endpoints);
    void UpdateJobQueue(const std::vector<Job>& jobs);

private:
    ServerStatusPanel* m_topPane;
    EndpointListPanel* m_leftPane;
    JobQueuePanel* m_rightPane;
    ApiClient* m_apiClient;
};
```

### EndpointListPanel Class

```cpp
class EndpointListPanel : public wxPanel {
public:
    EndpointListPanel(wxWindow* parent);

    // UI Actions
    void SetEndpoints(const std::vector<Endpoint>& endpoints);
    std::string GetSelectedEndpointId() const;

    // Drag-and-Drop Target
    bool OnDropJob(const std::string& jobId, const std::string& targetEndpointId);

private:
    wxListCtrl* m_listCtrl;
};
```

### JobQueuePanel Class

```cpp
class JobQueuePanel : public wxPanel {
public:
    JobQueuePanel(wxWindow* parent);

    // UI Actions
    void SetJobs(const std::vector<Job>& jobs);
    std::string GetSelectedJobId() const;

    // Drag Source
    void OnBeginDrag(wxListEvent& event);

private:
    wxDataViewCtrl* m_dataView;
    wxTextCtrl* m_filterInput;
};
```

## Data Models

### Endpoint Model

Represents a Transcoding Node in the cluster.

- **id**: Unique UUID string.
- **hostname**: Human-readable name.
- **status**: Enum (IDLE, BUSY, OFFLINE).
- **load**: Current CPU load percentage.

### Job Model

Represents a single transcoding task.

- **id**: Unique UUID string.
- **source_url**: URL of the input file.
- **target_codec**: Desired output format.
- **status**: Enum (PENDING, TRANSCODING, COMPLETED, FAILED).
- **progress**: Integer (0-100).
- **assigned_endpoint_id**: ID of the current worker node (if any).

### ServerMetrics Model

Global health indicators.

- **total_jobs**: Count of all jobs in db.
- **total_endpoints**: Count of registered nodes.
- **system_load**: Aggregate load average.
- **version**: Server version string.

## Error Handling

### Connection Failures

- **Initial Connection**: If the client cannot reach the Dispatch Server on startup, a modal dialog prompts the user to check settings or retry.
- **Runtime Disconnection**: If a periodic status update fails, the UI visually degrades (greys out) and the "Connected" indicator turns red. Background polling continues with exponential backoff.

### API Errors

- **4xx/5xx Responses**: API failures (e.g., "Reassignment Failed") trigger a `wxNotificationMessage` (toast) or a `wxMessageBox` depending on severity.
- **Invalid Data**: Malformed JSON responses are logged to stderr and ignored to prevent UI crashes.

## Testing Strategy

### Dual Testing Approach

The testing strategy employs both unit testing (using Google Test) and property-based testing (conceptual verification) to ensure robustness:

- **Unit tests** verify UI logic, data parsing, and API wrapper behavior.
- **Property-based tests** verify invariants such as "A job can never be assigned to an offline endpoint."

### Property Test Requirements

**Property 1: Configuration Persistence**
*For any* valid server URL and API key saved in settings, the next application launch MUST retrieve those exact same values.
**Validates: Requirement 1.3**

**Property 2: T-Split Invariance**
*For any* resize event of the main window, the relative existence of the Top, Left, and Right panes MUST be preserved (they should not disappear).
**Validates: Requirement 2.3**

**Property 3: Endpoint Status Consistency**
*For any* endpoint reported as "Offline" by the API, the UI MUST NOT allow a job to be dropped onto it.
**Validates: Requirement 6.3**

**Property 4: Job Filter Correctness**
*For any* list of jobs and any filter string S, the displayed list MUST ONLY contain jobs where the ID contains S.
**Validates: Requirement 7.2**

**Property 5: Unique Selection**
*For any* click event on the Endpoint List, only ONE endpoint can be selected at a time, and the Job Queue MUST update to match that selection.
**Validates: Requirements 4.5, 4.6**

**Property 6: Empty Queue State**
*For any* selected endpoint with zero jobs, the Job Queue panel MUST display a user-friendly "Empty" state, not a blank white void.
**Validates: Requirement 5.5**

**Property 7: Drag Validation**
*For any* drag operation initiated, if the source endpoint is the same as the target endpoint, the drop action MUST be invalid (no-op).
**Validates: Requirement 6.8**

**Property 8: Retry Logic**
*For any* job in a non-FAILED state, the "Retry" context menu option MUST be disabled or hidden.
**Validates: Requirement 10.1**

### Unit Testing

**Data Parsing Tests**:
- Parse valid JSON into Endpoint/Job objects.
- Handle missing JSON fields gracefully (default values).

**Settings Manager Tests**:
- Verify save/load to disk.
- Verify handling of corrupt config files.

**UI State Tests**:
- Verify logic for enabling/disabling "Retry" buttons based on job status.
- Verify filter logic matches substrings correctly.

## Performance Considerations

### UI Responsiveness

**Main Thread Hygiene**: No blocking network calls are allowed on the main thread. `std::async` or `wxThread` will be used for all CPR requests.

**Virtual Lists**: `wxListCtrl` and `wxDataViewCtrl` will be used in "Virtual" mode if job counts exceed 1000, requesting data only for visible rows.

### Network Efficiency

**Polling Interval**: Status updates occur every 5 seconds (configurable) to balance freshness with server load.

**Delta Updates**: Future optimizations may implement "Delta" APIs to fetch only changed jobs rather than reloading the full list every cycle.

## Integration with DistConv Ecosystem

### API Compatibility

The client expects a standard DistConv Dispatch Server API:
- `GET /status`: Global metrics.
- `GET /endpoints`: List of nodes.
- `GET /endpoints/{id}/jobs`: Jobs for a node.
- `POST /jobs/{id}/reassign`: Move job.

### Security

- **API Key**: Transmitted in the `X-API-Key` header over HTTPS.
- **TLS Verification**: Enabled by default; configurable CA path for self-signed certs.
