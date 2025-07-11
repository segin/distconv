# DistConv Dispatch Server

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/segin/distconv)
[![Test Coverage](https://img.shields.io/badge/tests-150%2B%20passing-brightgreen)](https://github.com/segin/distconv)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)](https://en.cppreference.com/w/cpp/17)

The DistConv Dispatch Server is the central coordinator for the distributed video transcoding system. It manages job queues, engine registration, intelligent scheduling, and provides a complete REST API for system interaction.

## 🏗️ Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│    Clients      │    │ Dispatch Server │    │   Transcoding   │
│                 │    │                 │    │    Engines      │
│ ┌─────────────┐ │    │ ┌─────────────┐ │    │ ┌─────────────┐ │
│ │Submit Jobs  │◄┼────┤►│Job Queue    │◄┼────┤►│Process Jobs │ │
│ │Check Status │ │    │ │Engine Mgmt  │ │    │ │Send Results │ │
│ │List Jobs    │ │    │ │Scheduling   │ │    │ │Heartbeats   │ │
│ └─────────────┘ │    │ │State Persist│ │    │ └─────────────┘ │
└─────────────────┘    │ └─────────────┘ │    └─────────────────┘
                       │ Thread-Safe     │
                       │ HTTP Server     │
                       └─────────────────┘
```

## 🚀 Features

### Core Functionality
- **🧠 Intelligent Job Scheduling**: Optimal engine assignment based on capabilities and performance
- **💾 Persistent State Management**: Survives server restarts with complete state recovery
- **🔒 Thread-Safe Operations**: Concurrent request handling with mutex protection
- **🌐 Complete REST API**: Full CRUD operations for jobs and engines
- **💓 Health Monitoring**: Engine heartbeat tracking and status management
- **🔄 Retry Logic**: Configurable retry attempts with exponential backoff
- **⚡ Performance Optimization**: Smart scheduling based on job size and engine benchmarks

### Production Features
- **🛡️ Security**: API key authentication and input validation
- **📊 Monitoring**: Comprehensive logging and status reporting
- **🔧 Configuration**: Command-line, environment variable, and file-based configuration
- **📈 Scalability**: Designed for high-throughput production environments
- **🧪 Reliability**: 150+ comprehensive tests covering all functionality

## 📋 Table of Contents

- [Quick Start](#-quick-start)
- [Building](#-building)
- [Installation](#-installation)
- [Configuration](#-configuration)
- [API Reference](#-api-reference)
- [Testing](#-testing)
- [Deployment](#-deployment)
- [Troubleshooting](#-troubleshooting)

## 🚀 Quick Start

### Prerequisites

**System Requirements:**
- Linux (Ubuntu 20.04+ / Debian 11+ / CentOS 8+)
- C++17 compiler (GCC 9+ or Clang 10+)
- CMake 3.10+
- 2+ GB RAM, 2+ CPU cores

**Dependencies:**
- httplib (included as header-only)
- nlohmann-json (system package)
- Google Test (for testing)
- SQLite 3.0+ (for state persistence)

### Quick Build

```bash
# Clone repository
git clone https://github.com/segin/distconv.git
cd distconv/dispatch_server_cpp

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run tests (150+ tests)
./dispatch_server_tests

# Start server
./dispatch_server_app --api-key your-secure-key
```

## 🛠️ Building

### Build Options

**Release Build (Recommended for production):**
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

**Debug Build (For development):**
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

**Build with Tests:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTING=ON ..
make -j$(nproc)
```

### Dependencies Installation

**Ubuntu/Debian:**
```bash
sudo apt update && sudo apt install -y \
  build-essential cmake pkg-config git \
  nlohmann-json3-dev libgtest-dev libgmock-dev \
  libsqlite3-dev
```

**CentOS/RHEL:**
```bash
sudo dnf install -y \
  gcc-c++ cmake pkgconfig git \
  json-devel gtest-devel gmock-devel \
  sqlite-devel
```

**Alpine Linux:**
```bash
apk add --no-cache \
  build-base cmake pkgconfig git \
  nlohmann-json-dev gtest-dev \
  sqlite-dev
```

### Build Targets

```bash
# Build main server application
make dispatch_server_app

# Build test suite
make dispatch_server_tests

# Build and run tests
make test

# Install to system
sudo make install
```

## 📦 Installation

### Option 1: Automated Installation

```bash
# Run the main project installer
curl -fsSL https://raw.githubusercontent.com/segin/distconv/main/deployment/install.sh | bash
```

### Option 2: Manual Installation

```bash
# Build the server
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Create installation directories
sudo mkdir -p /opt/distconv/server/{bin,config,logs,data}

# Install binaries
sudo cp dispatch_server_app /opt/distconv/server/bin/

# Install SystemD service
sudo cp ../deployment/distconv-dispatch.service /etc/systemd/system/
sudo systemctl daemon-reload

# Create service user
sudo useradd -r -s /bin/false distconv
sudo chown -R distconv:distconv /opt/distconv

# Start service
sudo systemctl enable distconv-dispatch
sudo systemctl start distconv-dispatch
```

### Option 3: Docker Installation

```bash
# Build Docker image
docker build -t distconv/dispatch-server .

# Run container
docker run -d --name dispatch-server \
  -p 8080:8080 \
  -v $(pwd)/config:/app/config \
  -v $(pwd)/logs:/app/logs \
  -v $(pwd)/data:/app/data \
  -e DISTCONV_API_KEY=your-secure-key \
  distconv/dispatch-server
```

## ⚙️ Configuration

### Command Line Options

```bash
./dispatch_server_app [OPTIONS]

Options:
  --port PORT              Server port (default: 8080)
  --api-key KEY           API key for authentication (required)
  --state-file FILE       State persistence file (default: server_state.json)
  --log-level LEVEL       Logging level (DEBUG, INFO, WARN, ERROR)
  --max-jobs NUM          Maximum concurrent jobs (default: 100)
  --help                  Show help message
  --version               Show version information

Examples:
  ./dispatch_server_app --port 9000 --api-key secret123
  ./dispatch_server_app --state-file /data/server.json --log-level DEBUG
```

### Environment Variables

```bash
# Required
export DISTCONV_API_KEY="your-secure-api-key"

# Optional
export DISTCONV_PORT="8080"
export DISTCONV_STATE_FILE="/opt/distconv/server/data/server_state.json"
export DISTCONV_LOG_LEVEL="INFO"
export DISTCONV_MAX_JOBS="100"
export DISTCONV_ENGINE_TIMEOUT="300"
```

### Configuration File

Create `/opt/distconv/server/config/server.conf`:

```ini
# DistConv Dispatch Server Configuration

# Security
api_key = your-secure-api-key

# Network
port = 8080
bind_address = 0.0.0.0

# State Management
state_file = /opt/distconv/server/data/server_state.json
auto_save_interval = 30

# Performance
max_concurrent_jobs = 100
engine_timeout_seconds = 300
job_queue_max_size = 1000

# Logging
log_level = INFO
log_file = /opt/distconv/server/logs/dispatch.log
log_rotation = daily
log_max_files = 30

# Engine Management
engine_heartbeat_timeout = 180
engine_benchmark_interval = 3600
auto_retry_failed_jobs = true
max_job_retries = 3
```

### Security Configuration

**API Key Management:**
```bash
# Generate secure API key
openssl rand -hex 32

# Set via environment (recommended)
export DISTCONV_API_KEY="$(openssl rand -hex 32)"

# Or via secure file
echo "$(openssl rand -hex 32)" | sudo tee /opt/distconv/server/config/api_key
sudo chmod 600 /opt/distconv/server/config/api_key
sudo chown distconv:distconv /opt/distconv/server/config/api_key
```

**Firewall Configuration:**
```bash
# Allow only necessary ports
sudo ufw allow 8080/tcp
sudo ufw enable

# Or with iptables
sudo iptables -A INPUT -p tcp --dport 8080 -j ACCEPT
```

## 🌐 API Reference

### Authentication

All API requests require the `X-API-Key` header:

```bash
curl -H "X-API-Key: your_api_key" http://localhost:8080/
```

### Job Management Endpoints

#### Submit Job

```http
POST /jobs/
Content-Type: application/json
X-API-Key: your_api_key

{
  "source_url": "http://example.com/input.mp4",
  "target_codec": "h264",
  "job_size": 100.5,
  "max_retries": 3
}
```

**Response (201 Created):**
```json
{
  "job_id": "1704067200123456_0",
  "source_url": "http://example.com/input.mp4",
  "target_codec": "h264", 
  "job_size": 100.5,
  "status": "pending",
  "assigned_engine": null,
  "output_url": null,
  "retries": 0,
  "max_retries": 3,
  "created_at": "2024-01-01T12:00:00Z"
}
```

#### Get Job Status

```http
GET /jobs/{job_id}
X-API-Key: your_api_key
```

**Response (200 OK):**
```json
{
  "job_id": "1704067200123456_0",
  "status": "processing",
  "assigned_engine": "engine-001",
  "progress": 45.2,
  "output_url": null,
  "created_at": "2024-01-01T12:00:00Z",
  "updated_at": "2024-01-01T12:05:30Z"
}
```

#### List All Jobs

```http
GET /jobs/
X-API-Key: your_api_key

# Optional query parameters:
# ?status=pending|processing|completed|failed
# ?limit=50
# ?offset=0
```

**Response (200 OK):**
```json
{
  "jobs": [
    {
      "job_id": "1704067200123456_0",
      "status": "completed",
      "assigned_engine": "engine-001",
      "output_url": "http://storage.example.com/output.mp4"
    }
  ],
  "total": 1,
  "limit": 50,
  "offset": 0
}
```

#### Update Job Status

```http
POST /jobs/{job_id}/complete
Content-Type: application/json
X-API-Key: your_api_key

{
  "output_url": "http://storage.example.com/output.mp4"
}
```

```http
POST /jobs/{job_id}/fail
Content-Type: application/json
X-API-Key: your_api_key

{
  "error_message": "Transcoding failed: codec not supported"
}
```

### Engine Management Endpoints

#### Register/Update Engine

```http
POST /engines/heartbeat
Content-Type: application/json
X-API-Key: your_api_key

{
  "engine_id": "engine-001",
  "engine_type": "transcoder",
  "supported_codecs": ["h264", "h265", "vp9"],
  "status": "idle",
  "storage_capacity_gb": 500.0,
  "streaming_support": true,
  "hostname": "worker-01.example.com",
  "benchmark_time": 125.5,
  "local_job_queue": ["job1", "job2"]
}
```

#### Get Job Assignment

```http
POST /assign_job/
Content-Type: application/json
X-API-Key: your_api_key

{
  "engine_id": "engine-001",
  "capabilities": ["h264", "h265"]
}
```

**Response (200 OK) - Job Available:**
```json
{
  "job_id": "1704067200123456_0",
  "source_url": "http://example.com/input.mp4",
  "target_codec": "h264",
  "job_size": 100.5
}
```

**Response (204 No Content) - No Jobs Available**

#### List All Engines

```http
GET /engines/
X-API-Key: your_api_key
```

**Response (200 OK):**
```json
{
  "engines": [
    {
      "engine_id": "engine-001",
      "status": "idle",
      "supported_codecs": ["h264", "h265"],
      "last_heartbeat": "2024-01-01T12:00:00Z",
      "jobs_completed": 42,
      "avg_benchmark_time": 125.5
    }
  ]
}
```

### System Endpoints

#### Health Check

```http
GET /health
```

**Response (200 OK):**
```json
{
  "status": "healthy",
  "version": "1.0.0",
  "uptime_seconds": 3661,
  "jobs_queued": 5,
  "engines_active": 3
}
```

#### Server Statistics

```http
GET /stats
X-API-Key: your_api_key
```

**Response (200 OK):**
```json
{
  "jobs": {
    "total": 1000,
    "pending": 5,
    "processing": 15,
    "completed": 950,
    "failed": 30
  },
  "engines": {
    "total": 10,
    "active": 8,
    "idle": 6,
    "busy": 2,
    "offline": 2
  },
  "performance": {
    "avg_job_time": 180.5,
    "jobs_per_hour": 120,
    "success_rate": 95.2
  }
}
```

## 🧪 Testing

### Comprehensive Test Suite

The dispatch server includes 150+ tests covering all functionality:

```bash
# Run all tests
./dispatch_server_tests

# Run specific test categories
./dispatch_server_tests --gtest_filter="*ApiTest*"
./dispatch_server_tests --gtest_filter="*ThreadSafety*"
./dispatch_server_tests --gtest_filter="*PersistentStorage*"
./dispatch_server_tests --gtest_filter="*JobManagement*"

# Run with verbose output
./dispatch_server_tests --gtest_verbose

# Generate coverage report
make coverage
```

### Test Categories

**API Tests (25+ tests):**
- HTTP endpoint functionality
- Request/response validation
- Error handling
- Authentication

**Thread Safety Tests (20+ tests):**
- Concurrent job submission
- Engine registration races
- State persistence under load
- Deadlock prevention

**Persistent Storage Tests (35+ tests):**
- State save/load functionality
- Recovery after crashes
- Data integrity validation
- Migration handling

**Job Management Tests (30+ tests):**
- Job lifecycle management
- Scheduling algorithms
- Retry logic
- Status transitions

**Engine Management Tests (20+ tests):**
- Engine registration
- Heartbeat processing
- Job assignment logic
- Performance tracking

**Edge Cases and Error Handling (20+ tests):**
- Malformed requests
- Resource exhaustion
- Network failures
- Invalid state transitions

### Performance Testing

```bash
# Load testing with Apache Bench
ab -n 1000 -c 10 -H "X-API-Key: test-key" \
   -T "application/json" \
   -p job_payload.json \
   http://localhost:8080/jobs/

# Stress testing
./dispatch_server_tests --gtest_filter="*Stress*"

# Memory leak detection
valgrind --tool=memcheck --leak-check=full ./dispatch_server_app
```

## 🚀 Deployment

### SystemD Service

**Service File (`/etc/systemd/system/distconv-dispatch.service`):**
```ini
[Unit]
Description=DistConv Dispatch Server
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=distconv
Group=distconv
WorkingDirectory=/opt/distconv/server
ExecStart=/opt/distconv/server/bin/dispatch_server_app \
  --port 8080 \
  --state-file /opt/distconv/server/data/server_state.json
Restart=always
RestartSec=10

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/opt/distconv/server/data /opt/distconv/server/logs

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

# Environment
Environment=DISTCONV_API_KEY_FILE=/opt/distconv/server/config/api_key
Environment=DISTCONV_LOG_LEVEL=INFO

[Install]
WantedBy=multi-user.target
```

**Service Management:**
```bash
# Enable and start
sudo systemctl enable distconv-dispatch
sudo systemctl start distconv-dispatch

# Check status
sudo systemctl status distconv-dispatch

# View logs
sudo journalctl -u distconv-dispatch -f

# Restart
sudo systemctl restart distconv-dispatch
```

### Docker Deployment

**Dockerfile:**
```dockerfile
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config git \
    nlohmann-json3-dev libgtest-dev

WORKDIR /app
COPY . .
RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

FROM ubuntu:22.04 AS runtime
RUN apt-get update && apt-get install -y \
    nlohmann-json3-dev && \
    rm -rf /var/lib/apt/lists/*

RUN groupadd -r distconv && useradd -r -g distconv distconv
WORKDIR /app
COPY --from=builder /app/build/dispatch_server_app /app/bin/

USER distconv
EXPOSE 8080
ENTRYPOINT ["/app/bin/dispatch_server_app"]
```

**Docker Compose:**
```yaml
version: '3.8'
services:
  dispatch-server:
    build: .
    ports:
      - "8080:8080"
    environment:
      - DISTCONV_API_KEY=${DISTCONV_API_KEY}
      - DISTCONV_LOG_LEVEL=INFO
    volumes:
      - ./data:/app/data
      - ./logs:/app/logs
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
```

### Production Deployment

**Load Balancer Configuration (nginx):**
```nginx
upstream distconv_dispatch {
    server 127.0.0.1:8080;
    server 127.0.0.1:8081;
    server 127.0.0.1:8082;
}

server {
    listen 80;
    server_name dispatch.example.com;

    location / {
        proxy_pass http://distconv_dispatch;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # Timeouts for long-running requests
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;
    }
    
    location /health {
        proxy_pass http://distconv_dispatch;
        access_log off;
    }
}
```

**Monitoring with Prometheus:**
```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'distconv-dispatch'
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: '/metrics'
    scrape_interval: 15s
```

## 🔧 Troubleshooting

### Common Issues

#### Server Won't Start

```bash
# Check configuration
./dispatch_server_app --help

# Validate dependencies
ldd dispatch_server_app

# Check port availability
netstat -tulpn | grep :8080

# Verify permissions
ls -la /opt/distconv/server/
sudo -u distconv touch /opt/distconv/server/data/test.txt
```

#### High Memory Usage

```bash
# Monitor memory
ps aux | grep dispatch_server_app
top -p $(pgrep dispatch_server_app)

# Check for memory leaks
valgrind --tool=memcheck --leak-check=full ./dispatch_server_app

# Analyze core dumps
gdb dispatch_server_app core
```

#### Performance Issues

```bash
# Check system resources
htop
iostat -x 1
netstat -i

# Monitor API performance
curl -w "@curl-format.txt" -H "X-API-Key: key" http://localhost:8080/health

# Profiling
perf record -g ./dispatch_server_app
perf report
```

#### Database Issues

```bash
# Check state file
file /opt/distconv/server/data/server_state.json
jq . /opt/distconv/server/data/server_state.json

# Backup and restore
cp server_state.json server_state.json.backup
# Restart server to recreate state file
```

### Debug Mode

```bash
# Enable debug logging
export DISTCONV_LOG_LEVEL=DEBUG
./dispatch_server_app --log-level DEBUG

# Run with debugger
gdb ./dispatch_server_app
(gdb) run --api-key test-key
(gdb) bt  # for stack trace on crash

# Attach to running process
sudo gdb -p $(pgrep dispatch_server_app)
```

### Log Analysis

```bash
# View recent logs
tail -f /opt/distconv/server/logs/dispatch.log

# Search for errors
grep -i error /opt/distconv/server/logs/dispatch.log

# Analyze patterns
awk '/ERROR/ {print $1, $2, $NF}' /opt/distconv/server/logs/dispatch.log

# Monitor in real-time
journalctl -u distconv-dispatch -f --since "1 hour ago"
```

## 📊 Performance Tuning

### System Optimization

```bash
# Increase file descriptor limits
echo "distconv soft nofile 65536" >> /etc/security/limits.conf
echo "distconv hard nofile 65536" >> /etc/security/limits.conf

# Optimize network stack
echo "net.core.somaxconn = 65536" >> /etc/sysctl.conf
echo "net.ipv4.tcp_max_syn_backlog = 65536" >> /etc/sysctl.conf
sysctl -p
```

### Application Tuning

**High-throughput configuration:**
```ini
# High-performance settings
max_concurrent_jobs = 500
engine_timeout_seconds = 60
job_queue_max_size = 5000
auto_save_interval = 10
thread_pool_size = 16
```

**Memory-optimized configuration:**
```ini
# Memory-conservative settings
max_concurrent_jobs = 50
job_queue_max_size = 200
auto_save_interval = 60
cleanup_interval = 300
```

## 🤝 Contributing

### Development Setup

```bash
# Clone and build
git clone https://github.com/segin/distconv.git
cd distconv/dispatch_server_cpp

# Install development tools
sudo apt install -y clang-format clang-tidy cppcheck

# Build with all warnings
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ALL_WARNINGS=ON ..
make -j$(nproc)
```

### Code Style

```bash
# Format code
find src/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Static analysis
clang-tidy src/*.cpp -- -std=c++17
cppcheck --enable=all src/
```

### Testing Guidelines

- Add tests for all new features
- Maintain 100% API endpoint coverage
- Include both positive and negative test cases
- Test thread safety for concurrent operations
- Validate error handling and edge cases

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](../LICENSE) file for details.

## 🆘 Support

- **Issues**: [GitHub Issues](https://github.com/segin/distconv/issues)
- **Documentation**: [Main Project README](../README.md)
- **API Documentation**: See API Reference section above

---

**DistConv Dispatch Server** - Built with modern C++17 for production reliability