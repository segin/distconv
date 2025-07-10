# DistConv: Distributed Video Transcoding System

DistConv is a comprehensive distributed video transcoding system consisting of a central dispatch server and multiple transcoding engines. The system provides intelligent job scheduling, robust state management, and high availability through a REST API architecture.

## System Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  Client Apps    │    │ Dispatch Server │    │ Transcoding     │
│                 │    │                 │    │ Engines         │
│ - Job Submit    │◄──►│ - Job Queue     │◄──►│ - Video         │
│ - Status Check  │    │ - Engine Mgmt   │    │   Processing    │
│ - Job Lists     │    │ - Scheduling    │    │ - Heartbeats    │
└─────────────────┘    │ - State Persist │    │ - Results       │
                       └─────────────────┘    └─────────────────┘
```

## Features

### Dispatch Server
- **Intelligent Job Scheduling**: Assigns jobs based on engine capabilities and performance
- **Persistent State Management**: Survives server restarts with full state recovery
- **Thread-Safe Operations**: Handles concurrent requests safely
- **Comprehensive API**: Full REST API for job and engine management
- **Health Monitoring**: Engine heartbeat monitoring and status tracking
- **Retry Logic**: Configurable retry attempts for failed jobs
- **Performance Optimization**: Smart scheduling based on job size and engine benchmarks

### Transcoding Engines
- **High Performance**: Optimized transcoding algorithms
- **Multiple Codec Support**: Supports H.264, VP9, AV1, and more
- **Streaming Support**: Large file streaming capabilities
- **Benchmark Reporting**: Performance metrics for optimal scheduling
- **Fault Tolerance**: Automatic failure reporting and recovery

## Quick Start

### Prerequisites

- Linux system (Ubuntu 20.04+ recommended)
- C++17 compiler (GCC 9+ or Clang 10+)
- CMake 3.10+
- Git

### Installation

#### Option 1: Automated Installation

```bash
# Clone the repository
git clone https://github.com/segin/distconv.git
cd distconv

# Run the installation script
sudo deployment/install.sh
```

#### Option 2: Manual Installation

```bash
# Clone the repository
git clone https://github.com/segin/distconv.git
cd distconv/dispatch_server_cpp

# Build the project
mkdir build && cd build
cmake ..
make -j$(nproc)

# Install to system
sudo mkdir -p /opt/distconv/server
sudo cp dispatch_server_app /opt/distconv/server/
sudo cp ../dispatch_server_core.h /opt/distconv/server/
sudo chown -R distconv:distconv /opt/distconv

# Create service user
sudo useradd -r -s /bin/false distconv

# Install systemd service
sudo cp ../deployment/distconv-dispatch.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable distconv-dispatch
sudo systemctl start distconv-dispatch
```

### Configuration

The dispatch server can be configured via command-line arguments or environment variables:

```bash
# Start with API key authentication
/opt/distconv/server/dispatch_server_app --api-key your_secure_api_key

# Or use environment variable
export DISTCONV_API_KEY=your_secure_api_key
/opt/distconv/server/dispatch_server_app
```

## API Usage

### Authentication

All API requests require an API key passed via the `X-API-Key` header:

```bash
curl -H "X-API-Key: your_api_key" http://localhost:8080/jobs/
```

### Submit a Transcoding Job

```bash
curl -X POST http://localhost:8080/jobs/ \
  -H "Content-Type: application/json" \
  -H "X-API-Key: your_api_key" \
  -d '{
    "source_url": "http://example.com/input.mp4",
    "target_codec": "h264",
    "job_size": 100.5,
    "max_retries": 3
  }'
```

Response:
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
  "max_retries": 3
}
```

### Check Job Status

```bash
curl -H "X-API-Key: your_api_key" \
  http://localhost:8080/jobs/1704067200123456_0
```

### List All Jobs

```bash
curl -H "X-API-Key: your_api_key" \
  http://localhost:8080/jobs/
```

### Register Transcoding Engine

```bash
curl -X POST http://localhost:8080/engines/heartbeat \
  -H "Content-Type: application/json" \
  -H "X-API-Key: your_api_key" \
  -d '{
    "engine_id": "engine-001",
    "engine_type": "transcoder",
    "supported_codecs": ["h264", "vp9"],
    "status": "idle",
    "storage_capacity_gb": 500.0,
    "streaming_support": true,
    "benchmark_time": 125.5
  }'
```

### Engine Job Assignment

```bash
curl -X POST http://localhost:8080/assign_job/ \
  -H "Content-Type: application/json" \
  -H "X-API-Key: your_api_key" \
  -d '{}'
```

### Complete a Job

```bash
curl -X POST http://localhost:8080/jobs/1704067200123456_0/complete \
  -H "Content-Type: application/json" \
  -H "X-API-Key: your_api_key" \
  -d '{
    "output_url": "http://example.com/output.mp4"
  }'
```

### Report Job Failure

```bash
curl -X POST http://localhost:8080/jobs/1704067200123456_0/fail \
  -H "Content-Type: application/json" \
  -H "X-API-Key: your_api_key" \
  -d '{
    "error_message": "Transcoding failed due to codec incompatibility"
  }'
```

## Job Scheduling

The dispatch server uses intelligent scheduling algorithms:

### Job Size Categories
- **Small jobs** (< 50 MB): Assigned to slowest available engine to preserve faster engines
- **Medium jobs** (50-100 MB): Assigned to fastest available engine
- **Large jobs** (≥ 100 MB): Preferred assignment to streaming-capable engines

### Engine Selection
1. Only "idle" engines are considered
2. Engines must have benchmark data for prioritization
3. Engines are ranked by performance (benchmark_time)
4. Streaming support is preferred for large jobs

## State Management

### Persistence
- All job and engine data persists to JSON files
- Automatic saves after every state change
- Atomic file operations prevent corruption
- Graceful recovery from file corruption or missing files

### State File Location
- Default: `dispatch_server_state.json` in working directory
- Production: `/opt/distconv/server/state.json`
- Configurable via environment variable: `DISTCONV_STATE_FILE`

### State File Format
```json
{
  "jobs": {
    "job_id_1": { /* job object */ },
    "job_id_2": { /* job object */ }
  },
  "engines": {
    "engine_id_1": { /* engine object */ },
    "engine_id_2": { /* engine object */ }
  }
}
```

## Monitoring and Logging

### Service Status
```bash
# Check service status
sudo systemctl status distconv-dispatch

# View logs
sudo journalctl -u distconv-dispatch -f

# Check service health
curl http://localhost:8080/
```

### Health Endpoints
- `GET /`: Basic health check (returns "OK")
- `GET /jobs/`: List all jobs (with API key)
- `GET /engines/`: List all engines (with API key)

### Performance Monitoring
- Monitor response times via application logs
- Track job completion rates
- Monitor engine utilization
- Alert on failed jobs or unresponsive engines

## Development

### Building from Source

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake git

# Clone and build
git clone https://github.com/segin/distconv.git
cd distconv/dispatch_server_cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Running Tests

```bash
# Build tests
cd build
make -j$(nproc)

# Run full test suite (150 tests)
./dispatch_server_tests

# Run specific test categories
./dispatch_server_tests --gtest_filter="*ApiTest*"
./dispatch_server_tests --gtest_filter="*EdgeCase*"
./dispatch_server_tests --gtest_filter="*ThreadSafety*"
```

### Test Coverage
The project includes a comprehensive test suite with 150 tests covering:
- All API endpoints and error cases
- Job state transitions and retry logic
- Engine management and scheduling
- Thread safety and concurrent operations
- Edge cases and robustness testing
- State persistence and recovery
- Performance under load

## Configuration Options

### Environment Variables
- `DISTCONV_API_KEY`: API key for authentication
- `DISTCONV_PORT`: Server port (default: 8080)
- `DISTCONV_STATE_FILE`: Path to state file
- `DISTCONV_LOG_LEVEL`: Logging verbosity (DEBUG, INFO, WARN, ERROR)

### Command Line Arguments
```bash
./dispatch_server_app --help
./dispatch_server_app --api-key <key>
./dispatch_server_app --port 8080
```

### Security Considerations
- Always use strong API keys in production
- Run service as non-root user (distconv)
- Use HTTPS in production environments
- Regularly rotate API keys
- Monitor access logs for suspicious activity

## Troubleshooting

### Common Issues

#### Server Won't Start
```bash
# Check port availability
sudo netstat -tlpn | grep :8080

# Check permissions
sudo chown -R distconv:distconv /opt/distconv
sudo chmod +x /opt/distconv/server/dispatch_server_app

# Check logs
sudo journalctl -u distconv-dispatch --no-pager -l
```

#### Jobs Not Being Assigned
```bash
# Check engine status
curl -H "X-API-Key: your_key" http://localhost:8080/engines/

# Verify engine heartbeats are being sent
# Ensure engines have benchmark_time data
# Check for pending jobs
curl -H "X-API-Key: your_key" http://localhost:8080/jobs/
```

#### State File Corruption
```bash
# Backup corrupted state
sudo cp /opt/distconv/server/state.json /opt/distconv/server/state.json.backup

# Restart with clean state (will lose job history)
sudo rm /opt/distconv/server/state.json
sudo systemctl restart distconv-dispatch

### Uninstallation

To completely remove DistConv:

```bash
# Run the uninstall script
sudo deployment/uninstall.sh
```

The uninstall script will:
- Stop and disable the service
- Optionally backup configuration and state files
- Remove all application files
- Clean up systemd configuration
- Optionally remove the system user

### Log Analysis
```bash
# Filter for errors
sudo journalctl -u distconv-dispatch | grep ERROR

# Monitor real-time activity
sudo journalctl -u distconv-dispatch -f

# Performance analysis
sudo journalctl -u distconv-dispatch | grep "ms total"
```

## Performance Tuning

### Hardware Recommendations

#### Minimum Requirements
- CPU: 2 cores
- RAM: 1 GB
- Storage: 10 GB
- Network: 100 Mbps

#### Recommended for Production
- CPU: 4+ cores
- RAM: 4+ GB
- Storage: 100+ GB SSD
- Network: 1+ Gbps

### Optimization Settings

#### For High-Volume Workloads
```bash
# Increase file descriptor limits
echo "distconv soft nofile 65536" | sudo tee -a /etc/security/limits.conf
echo "distconv hard nofile 65536" | sudo tee -a /etc/security/limits.conf

# Optimize systemd service
sudo systemctl edit distconv-dispatch
```

Add to override file:
```ini
[Service]
LimitNOFILE=65536
CPUAffinity=0-3
IOSchedulingClass=1
IOSchedulingPriority=4
```

## API Reference

Complete API documentation is available in [docs/protocol.md](docs/protocol.md).

### Quick Reference

| Method | Endpoint | Purpose |
|--------|----------|---------|
| POST | `/jobs/` | Submit new job |
| GET | `/jobs/` | List all jobs |
| GET | `/jobs/{id}` | Get job status |
| POST | `/jobs/{id}/complete` | Mark job complete |
| POST | `/jobs/{id}/fail` | Report job failure |
| POST | `/engines/heartbeat` | Engine registration |
| GET | `/engines/` | List engines |
| POST | `/engines/benchmark_result` | Submit benchmarks |
| POST | `/assign_job/` | Request job assignment |

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature-name`
3. Make changes and add tests
4. Run the test suite: `make test`
5. Commit changes: `git commit -am 'Add feature'`
6. Push to branch: `git push origin feature-name`
7. Submit a pull request

### Coding Standards
- Follow C++17 standards
- Include tests for all new functionality
- Update documentation for API changes
- Use consistent naming conventions
- Add appropriate error handling

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

- **Documentation**: [docs/](docs/)
- **Issues**: [GitHub Issues](https://github.com/segin/distconv/issues)
- **API Reference**: [docs/protocol.md](docs/protocol.md)
- **User Guide**: [docs/user_guide.md](docs/user_guide.md)

## Changelog

### v1.0.0 (Current)
- Complete 150-test validation suite
- Comprehensive REST API
- Intelligent job scheduling
- Persistent state management
- Thread-safe operations
- Production-ready systemd service
- Detailed documentation and monitoring

---

**DistConv** - Scalable, reliable, and intelligent distributed video transcoding.