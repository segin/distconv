# Distributed Video Transcoding System - User Guide

This guide provides instructions for setting up, building, and running the Distributed Video Transcoding System. It covers the Transcoding Engine, C++ Dispatch Server, and C++ Submission Client.

## 1. System Overview

The Distributed Video Transcoding System consists of three main components:

*   **Transcoding Engine (C++):** A daemon running on individual compute nodes responsible for performing the actual video transcoding using `ffmpeg`. It reports its status, capabilities (ffmpeg encoders, decoders, hardware acceleration), CPU temperature, and local job queue to the Dispatch Server.
*   **C++ Dispatch Server:** The central server that manages transcoding jobs, tracks the status and capabilities of Transcoding Engines, assigns jobs, and handles persistent storage of job and engine states. It exposes a REST API for communication.
*   **C++ Submission Client:** A desktop application (currently console-based, with plans for a GUI) that allows users to submit transcoding jobs, retrieve job statuses, list all jobs, and view the status of all connected Transcoding Engines.

## 2. Prerequisites

Before building and running the system, ensure you have the following prerequisites installed on your system:

### General Prerequisites

*   **Git:** For cloning the repository.
    ```bash
    sudo apt-get update
    sudo apt-get install -y git
    ```
*   **CMake (version 3.10 or higher):** For building the C++ components.
    ```bash
    sudo apt-get update
    sudo apt-get install -y cmake
    ```
*   **g++ (C++ Compiler):** A C++17 compatible compiler.
    ```bash
    sudo apt-get update
    sudo apt-get install -y build-essential
    ```
*   **curl:** Used by the Transcoding Engine for HTTP communication.
    ```bash
    sudo apt-get update
    sudo apt-get install -y curl
    ```
*   **libcurl4-openssl-dev:** Development files for libcurl, required for HTTPS communication.
    ```bash
    sudo apt-get update
    sudo apt-get install -y libcurl4-openssl-dev
    ```

### Transcoding Engine Specific Prerequisites

*   **FFmpeg:** The core transcoding tool. Ensure it's installed and accessible in your system's PATH.
    ```bash
    sudo apt-get update
    sudo apt-get install -y ffmpeg
    ```
*   **cJSON Development Files:** For JSON parsing in the Transcoding Engine.
    ```bash
    sudo apt-get update
    sudo apt-get install -y libcjson-dev
    ```
*   **SQLite3 Development Files:** For local job queue persistence in the Transcoding Engine.
    ```bash
    sudo apt-get update
    sudo apt-get install -y libsqlite3-dev
    ```

### C++ Submission Client Specific Prerequisites

*   **wxWidgets Development Files:** For the GUI (even for the console version, as it's linked).
    ```bash
    sudo apt-get update
    sudo apt-get install -y libwxgtk3.2-dev libwxbase3.2-dev
    ```

## 3. Quickstart

For a quick setup and demonstration, you can use the `quickstart.sh` script. This script will install necessary dependencies, generate SSL certificates, and build all C++ components.

1.  **Clone the repository:**
    ```bash
    git clone <repository_url>
    cd distconv
    ```
2.  **Run the quickstart script:**
    ```bash
    chmod +x quickstart.sh
    ./quickstart.sh
    ```
    The script will guide you through the process.

## 4. Building Components Manually

If you prefer to build each component manually, follow these steps:

### 4.1. Transcoding Engine

```bash
cd transcoding_engine
mkdir build
cd build
cmake ..
make
```

### 4.2. C++ Dispatch Server

```bash
cd dispatch_server_cpp
mkdir build
cd build
cmake ..
make
```

### 4.3. C++ Submission Client

```bash
cd submission_client_desktop
mkdir build
cd build
cmake ..
make
```

## 5. Running Components

### 5.1. C++ Dispatch Server

Start the server in a terminal:

```bash
cd distconv/dispatch_server_cpp/build
./dispatch_server_cpp
```

The server will listen on `http://localhost:8080`.

### 5.2. Transcoding Engine

Start the engine in a separate terminal:

```bash
cd distconv/transcoding_engine/build
./transcoding_engine --ca-cert ../../server.crt --dispatch-url https://localhost:8080 --api-key <your_engine_api_key>
```

This will start sending heartbeats and `ffmpeg` capabilities to the Dispatch Server.

### 5.3. C++ Submission Client

Run the client in another terminal:

```bash
cd distconv/submission_client_desktop/build
./submission_client_desktop --ca-cert ../../server.crt --dispatch-url https://localhost:8080 --api-key your-super-secret-api-key
```

Follow the interactive menu to submit jobs, check status, and list engines/jobs.

## 6. Running as a Daemon (Systemd)

To run the C++ Dispatch Server as a traditional Unix system daemon using `systemd`:

1.  **Create a dedicated user and group** (recommended for security):
    ```bash
    sudo groupadd --system distconv
    sudo useradd --system --no-create-home -g distconv distconv
    ```
2.  **Copy the service file:**
    ```bash
    sudo cp distconv-dispatch-server.service /etc/systemd/system/
    ```
3.  **Reload systemd and enable the service:**
    ```bash
    sudo systemctl daemon-reload
    sudo systemctl enable distconv-dispatch-server.service
    ```
4.  **Start the service:**
    ```bash
    sudo systemctl start distconv-dispatch-server.service
    ```
5.  **Check service status:**
    ```bash
    systemctl status distconv-dispatch-server.service
    ```

## 7. API Key Usage

The system uses a hardcoded API key (`"your-super-secret-api-key"`) for authentication. **For production environments, it is crucial to replace this with a secure method of handling API keys**, such as environment variables, a dedicated secrets management system, or a more robust authentication mechanism.

## 8. Troubleshooting

*   **"Command not found" for `cmake`, `g++`, `ffmpeg`, `curl`, `openssl`:** Ensure these tools are installed and in your system's PATH. Refer to the "Prerequisites" section.
*   **"Could NOT find wxWidgets" during CMake:** Ensure `libwxgtk3.2-dev` and `libwxbase3.2-dev` are installed. If you're on a different Linux distribution, the package names might vary.
*   **Compilation errors related to `httplib.h` or `nlohmann/json.hpp`:** Ensure your `CMakeLists.txt` files correctly specify the include directories for `third_party/httplib` and `third_party/json/single_include`.
*   **Connection refused:** Verify that the C++ Dispatch Server is running and listening on the correct port (8080).
*   **Invalid API Key:** Ensure the API key used by the client matches the one hardcoded in the server.
