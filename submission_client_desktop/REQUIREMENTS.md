# DISTCONV SUBMISSION CLIENT REQUIREMENTS

## Introduction

This specification defines the requirements for the redesigned user interface of the DistConv Submission Client, a desktop application built using C++ and the wxWidgets toolkit. The client serves as the primary control surface for the DistConv distributed transcoding system, allowing users to connect to a Dispatch Server, monitor system health, and actively manage transcoding resources.

The implementation introduces a specific "T-split" layout to maximize information density and usability:
- **Top Pane**: Global server status and metrics.
- **Bottom-Left Pane**: List of available Transcoding Endpoints (TEs).
- **Bottom-Right Pane**: Context-sensitive Job Queue for the selected endpoint.

This architecture enables granular management of individual endpoints, including the ability to manually reallocate computational resources by dragging and dropping jobs between endpoints. The application communicates with the Dispatch Server via REST API to perform these operations.

## Glossary

- **DistConv**: The distributed video transcoding system consisting of a Dispatch Server, Transcoding Engines, and Submission Clients.
- **Submission Client**: The desktop application (C++/wxWidgets) used by operators to submit jobs and manage the system.
- **Dispatch Server**: The central coordinator that manages job queues and assigns work to Transcoding Endpoints.
- **Transcoding Endpoint (TE)**: A compute node capable of processing video transcoding jobs.
- **T-Split Layout**: A user interface arrangement with a vertical split where the bottom half is further split horizontally, forming a "T" shape of three panes.
- **Job Queue**: An ordered list of transcoding jobs assigned to a specific Transcoding Endpoint.
- **API Key**: A security credential used to authenticate the Submission Client with the Dispatch Server.
- **Resource Reallocation**: The process of moving a pending job from one Transcoding Endpoint to another to balance load or prioritize tasks.
- **Settings Dialog**: A dedicated UI window for configuring application parameters like the Server URL and API Key.
- **Property-Based Testing**: A testing methodology that verifies the validity of a system by checking that certain properties hold true for many randomly generated inputs.

## Requirements

### Requirement 1: Configuration and Connection

**User Story:** As a system operator, I want to configure the Dispatch Server connection details securely, so that I can connect to different transcoding clusters without editing configuration files manually.

#### Acceptance Criteria

1. WHEN accessing the settings menu THEN the Submission Client SHALL open a dedicated Settings Dialog modal
2. WHEN entering configuration THEN the Submission Client SHALL accept a Dispatch Server URL and an API Key
3. WHEN saving settings THEN the Submission Client SHALL persist the configuration to a local configuration file in a platform-standard location
4. WHEN launching the application THEN the Submission Client SHALL automatically attempt to connect using the stored credentials
5. WHEN connection fails THEN the Submission Client SHALL display a modal error message with options to "Retry" or "Open Settings"
6. WHEN connection succeeds THEN the Submission Client SHALL transition to the main dashboard view
7. WHEN authenticating THEN the Submission Client SHALL include the API Key in the `X-API-Key` header of all requests
8. WHEN validating input THEN the Submission Client SHALL ensure the Server URL is a validly formatted HTTP/HTTPS string
9. WHEN validating input THEN the Submission Client SHALL prevent saving empty API keys

### Requirement 2: Main User Interface Layout

**User Story:** As a system operator, I want a clear, organized layout that shows me the big picture and details simultaneously, so that I can monitor the system efficiently.

#### Acceptance Criteria

1. WHEN displaying the main window THEN the Submission Client SHALL render a vertical split separating the Top Pane from the Bottom Area
2. WHEN rendering the Bottom Area THEN the Submission Client SHALL render a horizontal split separating the Bottom-Left Pane from the Bottom-Right Pane
3. WHEN resizing windows THEN the Submission Client SHALL maintain the T-split relative proportions
4. WHEN dragging splitters THEN the Submission Client SHALL allow the user to resize the panes dynamically
5. WHEN initializing the UI THEN the Submission Client SHALL populate the Top Pane with Server Status components
6. WHEN initializing the UI THEN the Submission Client SHALL populate the Bottom-Left Pane with the Transcoding Endpoints list
7. WHEN initializing the UI THEN the Submission Client SHALL populate the Bottom-Right Pane with the Job Queue view
8. WHEN applying themes THEN the Submission Client SHALL respect the system or user-configured color scheme (Light/Dark)

### Requirement 3: Server Status Monitoring

**User Story:** As a system operator, I want to see high-level server metrics, so that I can instantly judge the health and load of the transcoding cluster.

#### Acceptance Criteria

1. WHEN connected to the server THEN the Submission Client SHALL display "Connected" status in green text in the Top Pane
2. WHEN disconnected from the server THEN the Submission Client SHALL display "Disconnected" status in red text in the Top Pane
3. WHEN updating metrics THEN the Submission Client SHALL query the server every 5 seconds (configurable) for the total number of active jobs
4. WHEN updating metrics THEN the Submission Client SHALL query the server for the total number of connected Transcoding Endpoints (TEs)
5. WHEN updating metrics THEN the Submission Client SHALL query the server for the average system load (0.0 to 1.0 scale)
6. WHEN displaying info THEN the Submission Client SHALL show the connected Server Version string
7. WHEN rendering the Top Pane THEN the Submission Client SHALL display these metrics in a legible, dashboard-style format using gauge or text widgets
8. WHEN connection is lost THEN the Submission Client SHALL visually indicate the stale state of the metrics (e.g., via greying out)

### Requirement 4: Transcoding Endpoint Management

**User Story:** As a system operator, I want to see a list of all available transcoding nodes, so that I can identify which machines are idle, busy, or offline.

#### Acceptance Criteria

1. WHEN retrieving data THEN the Submission Client SHALL fetch the list of registered Transcoding Endpoints from the Dispatch Server
2. WHEN rendering the Bottom-Left Pane THEN the Submission Client SHALL display the Transcoding Endpoints in a selectable `wxListCtrl` or `wxDataViewCtrl`
3. WHEN displaying an endpoint THEN the Submission Client SHALL show its unique identifier or hostname
4. WHEN displaying an endpoint THEN the Submission Client SHALL visually indicate its current status using icons (Green=Idle, Orange=Busy, Red=Offline)
5. WHEN clicking an endpoint THEN the Submission Client SHALL mark it as the currently "Selected Endpoint"
6. WHEN selecting an endpoint THEN the Submission Client SHALL trigger an update of the Job Queue pane (Bottom-Right)
7. WHEN endpoints join or leave THEN the Submission Client SHALL refresh the list to reflect the current cluster state
8. WHEN sorting the list THEN the Submission Client SHALL allow sorting by Hostname or Status

### Requirement 5: Job Queue Management

**User Story:** As a system operator, I want to see the specific jobs assigned to a node, so that I can understand what a specific machine is working on.

#### Acceptance Criteria

1. WHEN a Transcoding Endpoint is selected THEN the Submission Client SHALL fetch the job queue specific to that endpoint
2. WHEN rendering the Bottom-Right Pane THEN the Submission Client SHALL display the list of jobs in a `wxDataViewCtrl` with columns for ID, Status, and Progress
3. WHEN displaying a job THEN the Submission Client SHALL show the Job ID, current status (Pending, Transcoding, Completed, Failed), and progress bar
4. WHEN no endpoint is selected THEN the Submission Client SHALL display a "Select an Endpoint to view jobs" prompt in the Bottom-Right Pane
5. WHEN the selected endpoint has no jobs THEN the Submission Client SHALL display an "Empty Queue" message
6. WHEN jobs change status THEN the Submission Client SHALL update the list to reflect the new state
7. WHEN viewing the queue THEN the Submission Client SHALL allow scrolling if the number of jobs exceeds the viewable area
8. WHEN selecting a different endpoint THEN the Submission Client SHALL clear the current queue view and load the new endpoint's jobs immediately

### Requirement 6: Resource Reallocation (Drag-and-Drop)

**User Story:** As a system operator, I want to drag a job from one machine to another, so that I can manually balance the workload or free up a machine for high-priority tasks.

#### Acceptance Criteria

1. WHEN selecting a job in the Bottom-Right Pane THEN the Submission Client SHALL allow the user to initiate a drag operation
2. WHEN dragging a job THEN the Submission Client SHALL visually indicate the job being moved (e.g., using a drag image of the Job ID)
3. WHEN dragging over the Bottom-Left Pane THEN the Submission Client SHALL highlight valid target Transcoding Endpoints
4. WHEN dropping a job onto a target endpoint THEN the Submission Client SHALL initiate the Job Transfer Logic
5. WHEN executing Job Transfer Logic THEN the Submission Client SHALL send a POST request to `/jobs/{id}/reassign` with the new endpoint ID
6. WHEN the transfer succeeds THEN the Submission Client SHALL remove the job from the source queue and refresh the target queue (if visible)
7. WHEN the transfer fails THEN the Submission Client SHALL display an error notification and keep the job in the original queue
8. WHEN validating the drop THEN the Submission Client SHALL prevent dropping a job onto its current source endpoint

### Requirement 7: Job Filtering and Sorting

**User Story:** As a system operator, I want to filter and sort the job queue, so that I can quickly find failed jobs or high-priority tasks in a busy queue.

#### Acceptance Criteria

1. WHEN viewing the Job Queue THEN the Submission Client SHALL provide a text input field for filtering by Job ID
2. WHEN entering text in the filter THEN the Submission Client SHALL update the list in real-time to show only matching jobs
3. WHEN clicking column headers THEN the Submission Client SHALL sort the jobs by that column (ID, Status, Progress) in ascending or descending order
4. WHEN sorting by Status THEN the Submission Client SHALL order jobs logically (e.g., Failed -> Pending -> Transcoding -> Completed)

### Requirement 8: Detailed Job View

**User Story:** As a system operator, I want to see full details of a specific job, so that I can debug parameters or view error logs.

#### Acceptance Criteria

1. WHEN double-clicking a job in the Job Queue THEN the Submission Client SHALL open a "Job Details" modal window
2. WHEN displaying details THEN the Submission Client SHALL show the full source URL, target codec, preset used, and submission timestamp
3. WHEN a job is failed THEN the Submission Client SHALL display the full error message or log provided by the server
4. WHEN viewing details THEN the Submission Client SHALL provide a "Copy to Clipboard" button for the Job ID and error logs

### Requirement 9: Notifications and Alerts

**User Story:** As a system operator, I want to be notified of critical events, so that I can react to failures even if the window is in the background.

#### Acceptance Criteria

1. WHEN a job transitions to "Failed" status THEN the Submission Client SHALL trigger a system desktop notification
2. WHEN a job transitions to "Completed" status THEN the Submission Client SHALL trigger a system desktop notification (if configured)
3. WHEN critical server connection is lost THEN the Submission Client SHALL display a persistent warning banner in the main window
4. WHEN configuring notifications THEN the Submission Client SHALL allow the user to toggle "Notify on Completion" and "Notify on Failure" independently

### Requirement 10: Retry and Failure Handling

**User Story:** As a system operator, I want to manually retry failed jobs, so that I can recover from transient errors without resubmitting the entire job.

#### Acceptance Criteria

1. WHEN a job is in "Failed" status THEN the Submission Client SHALL enable a "Retry Job" context menu option
2. WHEN selecting "Retry Job" THEN the Submission Client SHALL send a POST request to `/jobs/{id}/retry`
3. WHEN the retry is accepted THEN the Submission Client SHALL update the job status to "Pending"
4. WHEN a job is "Transcoding" THEN the Submission Client SHALL provide a "Cancel Job" context menu option
5. WHEN canceling a job THEN the Submission Client SHALL prompt for confirmation before sending the cancel request
