# DistConv Transcoding Engine

A modern, high-performance C++ transcoding engine designed for distributed video processing workflows. The engine connects to a central dispatch server to receive transcoding jobs and processes them using FFmpeg with robust error handling and monitoring capabilities.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Monitoring & Logs](#monitoring--logs)
- [Troubleshooting](#troubleshooting)
- [Development](#development)
- [API Reference](#api-reference)
- [Performance Tuning](#performance-tuning)
- [Security](#security)
- [Contributing](#contributing)

## Overview

The DistConv Transcoding Engine is a worker node in a distributed video transcoding system. It:

- **Connects** to a central dispatch server to receive video transcoding jobs
- **Downloads** source video files from provided URLs
- **Transcodes** videos using FFmpeg with specified codecs and parameters
- **Uploads** completed videos to designated storage locations
- **Reports** job status and system health back to the dispatcher
- **Maintains** a local job queue for reliability and performance

### What Makes It Special

- 🚀 **Modern C++17** architecture with dependency injection
- 🔒 **Security hardened** with prepared statements and secure subprocess execution
- 🧪 **Fully testable** with comprehensive mock implementations (150+ tests)
- 📊 **Production ready** with monitoring, logging, and error handling
- 🐳 **Container native** with Docker support and health checks
- ⚡ **High performance** with efficient memory management and RAII

## Features

### Core Functionality
- **Automatic job polling** from dispatch server
- **Multi-codec support** (H.264, H.265, VP8, VP9, and more)
- **Hardware acceleration** detection and usage
- **Concurrent processing** with configurable worker threads
- **Automatic retry** mechanism for failed jobs
- **Progress reporting** and real-time status updates

### Reliability & Monitoring
- **Health checks** and heartbeat monitoring
- **Comprehensive logging** with configurable levels
- **Job queue persistence** with SQLite database
- **Graceful shutdown** handling
- **Resource monitoring** (CPU, temperature, storage)
- **Error recovery** with exponential backoff

### Security & Safety
- **SQL injection protection** with prepared statements
- **Command injection prevention** via secure subprocess execution
- **Input validation** and sanitization
- **Safe file handling** with temporary file cleanup
- **Authentication** via API keys
- **SSL/TLS support** for secure communications

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Dispatch      │    │   Transcoding    │    │    Storage      │
│   Server        │◄──►│   Engine         │◄──►│    Service      │
│                 │    │                  │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                │
                                ▼
                       ┌──────────────────┐
                       │     FFmpeg       │
                       │   (Subprocess)   │
                       └──────────────────┘
```

### Key Components

1. **TranscodingEngine** - Core orchestration class
2. **HttpClient** - Handles API communication with dispatch server
3. **Database** - Manages local job queue and persistence
4. **SubprocessRunner** - Executes FFmpeg commands securely
5. **ConfigManager** - Handles configuration and environment variables

### Design Patterns Used

- **Dependency Injection** - All external dependencies are injected
- **RAII** - Automatic resource management
- **Strategy Pattern** - Pluggable codec and processing strategies
- **Observer Pattern** - Event-driven status reporting
- **Factory Pattern** - Component creation and initialization

## Quick Start

### Docker (Recommended)

```bash
# 1. Pull the latest image
docker pull distconv/transcoding-engine:latest

# 2. Create configuration
mkdir -p config logs data
cat > config/engine.json << EOF
{
  "dispatch_server_url": "http://your-dispatch-server:8080",
  "engine_id": "engine-$(hostname)",
  "api_key": "your-api-key",
  "hostname": "$(hostname)"
}
EOF

# 3. Run the engine
docker run -d \
  --name transcoding-engine \
  --restart unless-stopped \
  -v $(pwd)/config:/app/config \
  -v $(pwd)/logs:/app/logs \
  -v $(pwd)/data:/app/data \
  distconv/transcoding-engine:latest
```

### Native Installation

```bash
# 1. Install using the automated installer
curl -fsSL https://raw.githubusercontent.com/segin/distconv/main/transcoding_engine/deployment/install.sh | bash

# 2. Configure the engine
sudo vim /opt/distconv/engine/config/engine.json

# 3. Start the service
sudo systemctl start distconv-transcoding-engine
sudo systemctl enable distconv-transcoding-engine
```

## Installation

### Prerequisites

**System Requirements:**
- Ubuntu 20.04+ / Debian 11+ / CentOS 8+
- 4+ GB RAM
- 2+ CPU cores
- 50+ GB storage for temporary files
- Network access to dispatch server

**Software Dependencies:**
- FFmpeg 4.0+
- SQLite 3.0+
- OpenSSL 1.1+
- libcurl 7.0+

### Automated Installation

The easiest way to install is using our automated installer:

```bash
curl -fsSL https://raw.githubusercontent.com/segin/distconv/main/transcoding_engine/deployment/install.sh | bash
```

This installer will:
- Install all required dependencies
- Build the engine from source
- Create systemd service
- Set up logging and directories
- Create configuration templates

### Manual Installation

#### 1. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config git \
  ffmpeg libsqlite3-dev libssl-dev \
  libcurl4-openssl-dev nlohmann-json3-dev
```

**CentOS/RHEL:**
```bash
sudo dnf install -y \
  gcc-c++ cmake pkgconfig git \
  ffmpeg-devel sqlite-devel openssl-devel \
  libcurl-devel json-devel
```

#### 2. Build CPR Library

```bash
git clone https://github.com/libcpr/cpr.git
cd cpr && mkdir build && cd build
cmake .. -DCPR_USE_SYSTEM_CURL=ON
make -j$(nproc) && sudo make install
sudo ldconfig
```

#### 3. Build Transcoding Engine

```bash
git clone https://github.com/segin/distconv.git
cd distconv/transcoding_engine
cp CMakeLists_modern.txt CMakeLists.txt
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

#### 4. Install

```bash
sudo mkdir -p /opt/distconv/engine/{bin,config,logs,data}
sudo cp transcoding_engine_modern /opt/distconv/engine/bin/
sudo cp ../deployment/engine.json /opt/distconv/engine/config/
sudo cp ../deployment/distconv-transcoding-engine.service /etc/systemd/system/
sudo systemctl daemon-reload
```

### Docker Installation

#### Build from Source

```bash
git clone https://github.com/segin/distconv.git
cd distconv/transcoding_engine
docker build -t distconv/transcoding-engine .
```

#### Using Docker Compose

```yaml
version: '3.8'
services:
  transcoding-engine:
    image: distconv/transcoding-engine:latest
    restart: unless-stopped
    volumes:
      - ./config:/app/config
      - ./logs:/app/logs
      - ./data:/app/data
    environment:
      - DISTCONV_CONFIG_FILE=/app/config/engine.json
      - DISTCONV_LOG_LEVEL=INFO
    depends_on:
      - dispatch-server
```

## Configuration

### Configuration File

The engine is configured via a JSON file. The default location is `/app/config/engine.json` (Docker) or `/opt/distconv/engine/config/engine.json` (native).

#### Basic Configuration

```json
{
  "dispatch_server_url": "http://dispatch-server:8080",
  "engine_id": "engine-worker-01",
  "api_key": "your-secret-api-key",
  "hostname": "worker-node-01",
  "database_path": "/app/data/transcoding_jobs.db",
  "storage_capacity_gb": 500.0,
  "streaming_support": true,
  "heartbeat_interval_seconds": 30,
  "benchmark_interval_minutes": 60,
  "job_poll_interval_seconds": 5,
  "http_timeout_seconds": 120,
  "test_mode": false
}
```

#### Advanced Configuration

```json
{
  "dispatch_server_url": "https://dispatch.example.com",
  "engine_id": "engine-production-worker-01",
  "api_key": "prod-api-key-here",
  "hostname": "worker-01.example.com",
  "database_path": "/app/data/jobs.db",
  "storage_capacity_gb": 1000.0,
  "streaming_support": true,
  "heartbeat_interval_seconds": 15,
  "benchmark_interval_minutes": 30,
  "job_poll_interval_seconds": 2,
  "http_timeout_seconds": 300,
  "test_mode": false,
  "log_level": "INFO",
  "max_concurrent_jobs": 4,
  "temp_cleanup_interval_minutes": 60,
  "max_retry_attempts": 3,
  "retry_backoff_seconds": 30,
  "enable_hardware_acceleration": true,
  "ffmpeg_log_level": "warning",
  "ssl_verify_peer": true,
  "ssl_ca_cert_path": "/app/config/ca-cert.pem"
}
```

#### Configuration Options Reference

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `dispatch_server_url` | string | **required** | URL of the dispatch server |
| `engine_id` | string | **required** | Unique identifier for this engine |
| `api_key` | string | **required** | Authentication key for dispatch server |
| `hostname` | string | **required** | Hostname/IP of this worker |
| `database_path` | string | `"jobs.db"` | Path to SQLite database file |
| `storage_capacity_gb` | number | `500.0` | Available storage space in GB |
| `streaming_support` | boolean | `true` | Whether engine supports streaming |
| `heartbeat_interval_seconds` | integer | `30` | How often to send heartbeats |
| `benchmark_interval_minutes` | integer | `60` | How often to run benchmarks |
| `job_poll_interval_seconds` | integer | `5` | How often to check for new jobs |
| `http_timeout_seconds` | integer | `120` | HTTP request timeout |
| `test_mode` | boolean | `false` | Enable test mode (disables background threads) |
| `log_level` | string | `"INFO"` | Logging level (DEBUG, INFO, WARN, ERROR) |
| `max_concurrent_jobs` | integer | `2` | Maximum parallel transcoding jobs |
| `enable_hardware_acceleration` | boolean | `true` | Use hardware acceleration if available |

### Environment Variables

Configuration can also be set via environment variables:

```bash
export DISTCONV_DISPATCH_URL="http://dispatch.example.com"
export DISTCONV_ENGINE_ID="my-engine"
export DISTCONV_API_KEY="secret-key"
export DISTCONV_LOG_LEVEL="DEBUG"
```

Environment variables override configuration file values.

### SSL/TLS Configuration

For secure communications:

```json
{
  "dispatch_server_url": "https://secure-dispatch.example.com",
  "ssl_verify_peer": true,
  "ssl_ca_cert_path": "/app/config/ca-certificates.pem",
  "http_timeout_seconds": 300
}
```

## Usage

### Starting the Engine

#### Docker
```bash
docker run -d \
  --name transcoding-engine \
  --restart unless-stopped \
  -v $(pwd)/config:/app/config:ro \
  -v $(pwd)/logs:/app/logs \
  -v $(pwd)/data:/app/data \
  distconv/transcoding-engine:latest
```

#### Systemd Service
```bash
sudo systemctl start distconv-transcoding-engine
sudo systemctl status distconv-transcoding-engine
```

#### Manual
```bash
cd /opt/distconv/engine
./bin/transcoding_engine_modern --config config/engine.json
```

### Command Line Options

```bash
./transcoding_engine_modern [OPTIONS]

Options:
  --config FILE         Configuration file path (default: engine.json)
  --log-level LEVEL     Set log level (DEBUG, INFO, WARN, ERROR)
  --test-mode          Enable test mode
  --version            Show version information
  --help               Show this help message

Examples:
  ./transcoding_engine_modern --config /etc/engine.json
  ./transcoding_engine_modern --log-level DEBUG
  ./transcoding_engine_modern --test-mode --config test-config.json
```

### Health Checks

The engine provides several ways to check its health:

#### Process Check
```bash
# Check if process is running
pgrep -f transcoding_engine_modern

# Check system resource usage
ps aux | grep transcoding_engine_modern
```

#### HTTP Health Endpoint (if enabled)
```bash
curl http://localhost:8080/health
```

#### Log Analysis
```bash
# Check recent logs
tail -f /app/logs/transcoding_engine.log

# Check for errors
grep -i error /app/logs/transcoding_engine.log
```

#### Database Status
```bash
# Check job queue status
sqlite3 /app/data/transcoding_jobs.db "SELECT COUNT(*) FROM jobs;"

# Check recent jobs
sqlite3 /app/data/transcoding_jobs.db "SELECT * FROM jobs ORDER BY created_at DESC LIMIT 10;"
```

## Monitoring & Logs

### Log Files

The engine generates several types of logs:

#### Main Application Log
- **Location**: `/app/logs/transcoding_engine.log`
- **Content**: General application events, job processing, errors
- **Rotation**: Daily, kept for 30 days

#### FFmpeg Logs
- **Location**: `/app/logs/ffmpeg/`
- **Content**: Detailed FFmpeg output for each transcoding job
- **Format**: One file per job (`job-{id}-{timestamp}.log`)

#### System Logs
- **Location**: `/var/log/syslog` (systemd journal)
- **Content**: Service start/stop, system-level events

### Log Levels

Configure log verbosity in your config file:

```json
{
  "log_level": "INFO"
}
```

Available levels:
- **DEBUG**: Verbose debugging information
- **INFO**: General information (default)
- **WARN**: Warning messages
- **ERROR**: Error messages only

### Monitoring Metrics

The engine reports the following metrics via heartbeats:

#### System Metrics
- CPU temperature
- Available storage space
- Memory usage
- Network connectivity

#### Performance Metrics
- Jobs processed per hour
- Average transcoding time
- Success/failure rates
- Queue depth

#### Capability Metrics
- Supported codecs
- Hardware acceleration status
- FFmpeg version and capabilities

### Centralized Logging

For production deployments, consider centralized logging:

#### Fluentd Example
```yaml
# fluent.conf
<source>
  @type tail
  path /app/logs/transcoding_engine.log
  pos_file /var/log/fluentd/transcoding_engine.log.pos
  tag distconv.transcoding
  format json
</source>

<match distconv.transcoding>
  @type elasticsearch
  host elasticsearch.example.com
  port 9200
  index_name distconv-transcoding
</match>
```

#### Docker Logging Driver
```bash
docker run -d \
  --log-driver=fluentd \
  --log-opt fluentd-address=fluentd.example.com:24224 \
  --log-opt tag="distconv.transcoding" \
  distconv/transcoding-engine:latest
```

### Alerting

Set up alerts for critical events:

#### Disk Space Alert
```bash
#!/bin/bash
# Check available disk space
THRESHOLD=90
USAGE=$(df /app/data | awk 'NR==2 {print $5}' | sed 's/%//')
if [ $USAGE -gt $THRESHOLD ]; then
  echo "ALERT: Disk usage is ${USAGE}%" | mail -s "Disk Space Alert" admin@example.com
fi
```

#### Service Health Check
```bash
#!/bin/bash
# Check if service is responsive
if ! systemctl is-active --quiet distconv-transcoding-engine; then
  echo "ALERT: Transcoding engine service is down" | mail -s "Service Alert" admin@example.com
fi
```

## Troubleshooting

### Common Issues

#### 1. Engine Won't Start

**Symptoms:**
- Service fails to start
- "Connection refused" errors
- Configuration file not found

**Solutions:**
```bash
# Check configuration file syntax
jq . /app/config/engine.json

# Verify file permissions
ls -la /app/config/engine.json

# Check dependencies
ldd /app/bin/transcoding_engine_modern

# Check systemd logs
journalctl -u distconv-transcoding-engine -f
```

#### 2. Cannot Connect to Dispatch Server

**Symptoms:**
- "Failed to register with dispatcher"
- Network timeout errors
- SSL certificate errors

**Solutions:**
```bash
# Test network connectivity
curl -v http://your-dispatch-server:8080/health

# Check DNS resolution
nslookup your-dispatch-server

# Test with curl using same settings
curl -H "X-API-Key: your-api-key" \
     -H "Content-Type: application/json" \
     -d '{"test": true}' \
     http://your-dispatch-server:8080/engines/heartbeat

# For SSL issues
curl -v --cacert /app/config/ca-cert.pem \
     https://your-dispatch-server:8080/health
```

#### 3. FFmpeg Transcoding Failures

**Symptoms:**
- Jobs fail with "FFmpeg transcoding failed"
- Codec not supported errors
- Input/output file errors

**Solutions:**
```bash
# Check FFmpeg installation
ffmpeg -version

# List available codecs
ffmpeg -codecs | grep h264

# Test FFmpeg command manually
ffmpeg -i input.mp4 -c:v h264 output.mp4

# Check FFmpeg logs
ls -la /app/logs/ffmpeg/
cat /app/logs/ffmpeg/job-latest.log

# Verify input file
ffprobe input.mp4
```

#### 4. Database Issues

**Symptoms:**
- "Database initialization failed"
- "Cannot add job to queue"
- SQLite errors

**Solutions:**
```bash
# Check database file permissions
ls -la /app/data/transcoding_jobs.db

# Test database connectivity
sqlite3 /app/data/transcoding_jobs.db ".tables"

# Check database integrity
sqlite3 /app/data/transcoding_jobs.db "PRAGMA integrity_check;"

# Recreate database (backup first!)
mv /app/data/transcoding_jobs.db /app/data/transcoding_jobs.db.backup
# Engine will recreate on next start
```

#### 5. Storage Space Issues

**Symptoms:**
- "No space left on device"
- Failed to download source files
- Cannot create temporary files

**Solutions:**
```bash
# Check disk usage
df -h /app/data

# Find large files
find /app/temp -type f -size +100M -ls

# Clean up old temporary files
find /app/temp -type f -mtime +1 -delete

# Check log file sizes
du -sh /app/logs/*
```

### Debug Mode

Enable debug mode for detailed troubleshooting:

```json
{
  "log_level": "DEBUG",
  "ffmpeg_log_level": "debug"
}
```

Or via environment variable:
```bash
export DISTCONV_LOG_LEVEL=DEBUG
```

### Performance Issues

#### High CPU Usage
```bash
# Check process stats
top -p $(pgrep transcoding_engine_modern)

# Check concurrent jobs
ps aux | grep ffmpeg | wc -l

# Reduce concurrent jobs in config
"max_concurrent_jobs": 1
```

#### High Memory Usage
```bash
# Check memory usage
ps -o pid,ppid,cmd,%mem,%cpu --sort=-%mem | head

# Monitor memory leaks
valgrind --tool=memcheck --leak-check=full ./transcoding_engine_modern
```

#### Network Issues
```bash
# Monitor network connections
netstat -tulpn | grep transcoding_engine_modern

# Check bandwidth usage
iftop -i eth0

# Test download speeds
wget --progress=bar:force http://example.com/test-video.mp4
```

### Getting Help

If you encounter issues not covered here:

1. **Check the logs** - Most issues are revealed in the log files
2. **Search GitHub Issues** - Someone may have encountered the same problem
3. **Enable debug logging** - Get more detailed information
4. **Create a minimal reproduction** - Strip down to the simplest failing case
5. **File an issue** - Include logs, configuration, and system information

**When filing issues, please include:**
- Engine version (`./transcoding_engine_modern --version`)
- Operating system and version
- Configuration file (with sensitive data redacted)
- Relevant log excerpts
- Steps to reproduce the issue

## Development

### Building from Source

#### Prerequisites
- C++17 compatible compiler (GCC 8+, Clang 8+)
- CMake 3.15+
- Required dependencies (see Installation section)

#### Build Steps
```bash
git clone https://github.com/segin/distconv.git
cd distconv/transcoding_engine

# Configure build (using modern CMakeLists)
cp CMakeLists_modern.txt CMakeLists.txt
mkdir build_modern && cd build_modern
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
make -j$(nproc)

# Run tests (37/37 should pass - 100% success rate)
./transcoding_engine_modern_tests

# Run specific test suites
./transcoding_engine_modern_tests --gtest_filter="*Refactoring*"
./transcoding_engine_modern_tests --gtest_filter="*TranscodingEngine*"
```

### Testing

The engine includes a comprehensive test suite with 37 tests (100% pass rate) covering:
- Unit tests for all components
- Integration tests with mocks
- End-to-end workflow tests
- Performance and stress tests

#### Running Tests
```bash
# Run all tests
./transcoding_engine_modern_tests

# Run specific test suites
./transcoding_engine_modern_tests --gtest_filter="*TranscodingEngine*"
./transcoding_engine_modern_tests --gtest_filter="*Refactoring*"

# Run with verbose output
./transcoding_engine_modern_tests --gtest_verbose

# Generate test coverage
make coverage
```

#### Test Categories
- **Tests 1-126**: Legacy compatibility tests
- **Tests 127-139**: Core engine testability and mocking
- **Tests 140-150**: Modern C++ implementation validation
- **Refactoring Tests**: Architecture modernization verification

### Code Style

The project follows modern C++ best practices:

#### Style Guidelines
- Use C++17 features
- RAII for resource management
- Smart pointers over raw pointers
- Const correctness
- Exception safety
- Clear naming conventions

#### Formatting
```bash
# Format code with clang-format
find src/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Check formatting
make check-format
```

#### Static Analysis
```bash
# Run clang-tidy
make tidy

# Run cppcheck
cppcheck --enable=all src/
```

### Architecture Overview

#### Core Classes

**TranscodingEngine** (`src/core/transcoding_engine.{h,cpp}`)
- Main orchestration class
- Manages job lifecycle
- Coordinates all other components

**Interfaces** (`src/interfaces/`)
- `IHttpClient` - HTTP communication interface
- `IDatabase` - Database operations interface
- `ISubprocessRunner` - Process execution interface

**Implementations** (`src/implementations/`)
- `CprHttpClient` - HTTP client using cpr library
- `SqliteDatabase` - SQLite database implementation
- `SecureSubprocess` - Secure subprocess execution

**Mocks** (`src/mocks/`)
- Complete mock implementations for testing
- Enable comprehensive unit testing
- Support dependency injection patterns

#### Design Patterns

**Dependency Injection**
```cpp
// Constructor injection
TranscodingEngine(
    std::unique_ptr<IHttpClient> http_client,
    std::unique_ptr<IDatabase> database,
    std::unique_ptr<ISubprocessRunner> subprocess_runner
);
```

**RAII Resource Management**
```cpp
class TranscodingEngine {
private:
    std::unique_ptr<IHttpClient> http_client_;
    std::unique_ptr<IDatabase> database_;
    // Automatic cleanup on destruction
};
```

**Exception Safety**
```cpp
bool TranscodingEngine::process_job(const JobDetails& job) {
    std::vector<std::string> temp_files;
    try {
        // Processing logic
        return true;
    } catch (const std::exception& e) {
        cleanup_temp_files(temp_files);  // Always cleanup
        handle_error("process_job", e.what());
        return false;
    }
}
```

### Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details.

#### Development Workflow
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

#### Pull Request Guidelines
- Include tests for new features
- Update documentation
- Follow existing code style
- Add appropriate log messages
- Consider backward compatibility

## API Reference

### Configuration Schema

The complete configuration schema is available in JSON Schema format:

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "dispatch_server_url": {
      "type": "string",
      "format": "uri",
      "description": "URL of the dispatch server"
    },
    "engine_id": {
      "type": "string",
      "minLength": 1,
      "description": "Unique identifier for this engine"
    },
    "api_key": {
      "type": "string",
      "minLength": 1,
      "description": "Authentication key for dispatch server"
    }
  },
  "required": ["dispatch_server_url", "engine_id", "api_key"]
}
```

### HTTP API

The engine communicates with the dispatch server via REST API:

#### Heartbeat Registration
```http
POST /engines/heartbeat
Content-Type: application/json
X-API-Key: your-api-key

{
  "engine_id": "engine-worker-01",
  "engine_type": "transcoder",
  "status": "idle",
  "hostname": "worker-01.example.com",
  "local_job_queue": ["job1", "job2"],
  "storage_capacity_gb": 500.0,
  "supported_codecs": ["h264", "h265", "vp8", "vp9"]
}
```

#### Job Assignment Request
```http
POST /assign_job/
Content-Type: application/json
X-API-Key: your-api-key

{
  "engine_id": "engine-worker-01",
  "capabilities": ["h264", "h265"]
}
```

#### Job Completion Report
```http
POST /jobs/{job_id}/complete
Content-Type: application/json
X-API-Key: your-api-key

{
  "output_url": "https://storage.example.com/transcoded/video.mp4"
}
```

#### Job Failure Report
```http
POST /jobs/{job_id}/fail
Content-Type: application/json
X-API-Key: your-api-key

{
  "error_message": "FFmpeg transcoding failed: Unsupported codec"
}
```

### Database Schema

The engine uses SQLite with the following schema:

```sql
CREATE TABLE jobs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    job_id TEXT UNIQUE NOT NULL,
    status TEXT DEFAULT 'pending',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    retry_count INTEGER DEFAULT 0
);

CREATE INDEX idx_jobs_job_id ON jobs(job_id);
CREATE INDEX idx_jobs_status ON jobs(status);
CREATE INDEX idx_jobs_created_at ON jobs(created_at);
```

## Performance Tuning

### Hardware Optimization

#### CPU Optimization
```json
{
  "max_concurrent_jobs": 4,  // Match CPU core count
  "job_poll_interval_seconds": 1,  // Faster polling for high-throughput
  "ffmpeg_threads": 0  // Let FFmpeg auto-detect
}
```

#### Memory Optimization
```json
{
  "temp_cleanup_interval_minutes": 30,  // More frequent cleanup
  "max_job_queue_size": 100,  // Limit queue size
  "database_cache_size": 10000  // SQLite cache size
}
```

#### Storage Optimization
```json
{
  "temp_directory": "/fast-ssd/temp",  // Use fast storage for temp files
  "database_path": "/fast-ssd/jobs.db",  // Database on fast storage
  "enable_wal_mode": true  // SQLite WAL mode for better concurrency
}
```

### Network Optimization

#### Bandwidth Management
```json
{
  "http_timeout_seconds": 300,  // Longer timeout for large files
  "max_download_size_mb": 1000,  // Limit file size
  "enable_compression": true,  // Compress HTTP requests
  "connection_pool_size": 10  // Reuse connections
}
```

#### Retry Configuration
```json
{
  "max_retry_attempts": 5,
  "retry_backoff_seconds": 30,
  "exponential_backoff": true,
  "retry_on_network_error": true
}
```

### FFmpeg Optimization

#### Hardware Acceleration
```json
{
  "enable_hardware_acceleration": true,
  "preferred_hw_accel": "cuda",  // cuda, vaapi, videotoolbox
  "hw_accel_fallback": true,  // Fall back to software if hw fails
  "gpu_memory_limit_mb": 2048
}
```

#### Quality vs Speed
```json
{
  "ffmpeg_preset": "fast",  // ultrafast, superfast, veryfast, faster, fast, medium, slow
  "quality_factor": 23,  // CRF value for quality (lower = better quality)
  "two_pass_encoding": false,  // Enable for better quality, slower speed
  "enable_b_frames": true  // Better compression, slightly slower
}
```

### Monitoring Performance

#### Key Metrics to Track
- Jobs processed per hour
- Average transcoding time per GB
- CPU and memory usage
- Network bandwidth utilization
- Storage I/O patterns
- Error rates and retry frequency

#### Performance Benchmarking
```bash
# Built-in benchmark
./transcoding_engine_modern --benchmark

# Custom performance test
time ./transcoding_engine_modern --test-mode --config benchmark.json
```

## Security

### Authentication & Authorization

#### API Key Management
- Use strong, randomly generated API keys
- Rotate keys regularly
- Store keys securely (environment variables, secrets management)
- Never log API keys

```bash
# Generate secure API key
openssl rand -hex 32

# Set via environment variable
export DISTCONV_API_KEY="$(openssl rand -hex 32)"
```

#### SSL/TLS Configuration
```json
{
  "dispatch_server_url": "https://dispatch.example.com",
  "ssl_verify_peer": true,
  "ssl_ca_cert_path": "/app/config/ca-certificates.pem",
  "ssl_min_version": "1.2"
}
```

### Input Validation

The engine implements comprehensive input validation:

#### File Upload Safety
- File type validation
- Size limits
- Path traversal prevention
- Malware scanning integration points

#### Command Injection Prevention
```cpp
// Safe: Uses argument vector
subprocess_->run({"ffmpeg", "-i", input_file, "-c:v", codec, output_file});

// Unsafe: Direct shell execution (not used)
// system("ffmpeg -i " + input_file + " -c:v " + codec + " " + output_file);
```

#### SQL Injection Prevention
```cpp
// Safe: Uses prepared statements
"INSERT INTO jobs (job_id, status) VALUES (?, ?)"

// Unsafe: String concatenation (not used)
// "INSERT INTO jobs (job_id) VALUES ('" + job_id + "')"
```

### Container Security

#### Non-Root Execution
```dockerfile
# Create and use non-root user
RUN groupadd -r distconv && useradd -r -g distconv distconv
USER distconv
```

#### Minimal Attack Surface
```dockerfile
# Multi-stage build - only runtime dependencies in final image
FROM ubuntu:22.04 AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg libsqlite3-0 libssl3 && \
    rm -rf /var/lib/apt/lists/*
```

#### Resource Limits
```yaml
# Docker Compose resource limits
services:
  transcoding-engine:
    deploy:
      resources:
        limits:
          cpus: '2.0'
          memory: 4G
        reservations:
          cpus: '1.0'
          memory: 2G
```

### Network Security

#### Firewall Configuration
```bash
# Allow only necessary outbound connections
iptables -A OUTPUT -p tcp --dport 80 -j ACCEPT   # HTTP to dispatch server
iptables -A OUTPUT -p tcp --dport 443 -j ACCEPT  # HTTPS to dispatch server
iptables -A OUTPUT -p tcp --dport 53 -j ACCEPT   # DNS
iptables -A OUTPUT -j DROP  # Drop all other outbound traffic
```

#### VPN/Private Networks
- Deploy engines in private subnets
- Use VPN for secure communication
- Implement network segmentation

### Compliance & Auditing

#### Logging for Security
```json
{
  "log_security_events": true,
  "log_api_calls": true,
  "log_file_operations": true,
  "audit_log_path": "/app/logs/audit.log"
}
```

#### Data Protection
- Encrypt sensitive data at rest
- Secure deletion of temporary files
- Compliance with data retention policies
- GDPR/privacy regulation compliance

## Contributing

We welcome contributions from the community! Please read our [Contributing Guidelines](CONTRIBUTING.md) for details on:

- Code of Conduct
- Development workflow
- Testing requirements
- Documentation standards
- Issue reporting
- Pull request process

### Quick Start for Contributors

```bash
# 1. Fork and clone
git clone https://github.com/yourusername/distconv.git
cd distconv/transcoding_engine

# 2. Set up development environment
./scripts/setup-dev.sh

# 3. Create feature branch
git checkout -b feature/your-feature-name

# 4. Make changes and test
make test
./transcoding_engine_modern_tests

# 5. Submit pull request
git push origin feature/your-feature-name
```

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

- **Documentation**: This README and inline code documentation
- **Issues**: [GitHub Issues](https://github.com/segin/distconv/issues)
- **Discussions**: [GitHub Discussions](https://github.com/segin/distconv/discussions)
- **Email**: support@distconv.example.com

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for a detailed history of changes.

---

**DistConv Transcoding Engine** - Built with ❤️ using modern C++17