# Deployment Guide

This directory contains deployment configurations for the Distconv transcoding system.

## Docker Deployment

### Prerequisites
- Docker 20.10+
- Docker Compose 2.0+

### Quick Start with Docker Compose

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd distconv
   ```

2. **Build and start all services**:
   ```bash
   cd deployment/docker
   docker-compose up -d
   ```

3. **Access the services**:
   - Dispatch Server: http://localhost:8080
   - Submission Client: GUI application (requires X11 forwarding)

### Individual Container Deployment

**Build images**:
```bash
# Dispatch Server
docker build -f deployment/docker/Dockerfile.dispatch-server -t distconv/dispatch-server .

# Transcoding Engine
docker build -f deployment/docker/Dockerfile.transcoding-engine -t distconv/transcoding-engine .

# Submission Client
docker build -f deployment/docker/Dockerfile.submission-client -t distconv/submission-client .
```

**Run containers**:
```bash
# Start dispatch server
docker run -d --name dispatch-server \
  -p 8080:8080 \
  -e API_KEY=your_api_key_here \
  distconv/dispatch-server

# Start transcoding engine
docker run -d --name transcoding-engine \
  -e DISPATCH_SERVER_URL=http://dispatch-server:8080 \
  --link dispatch-server \
  distconv/transcoding-engine

# Start submission client (with X11 forwarding)
docker run -d --name submission-client \
  -e DISPATCH_SERVER_URL=http://dispatch-server:8080 \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
  --link dispatch-server \
  distconv/submission-client
```

## Kubernetes Deployment

### Prerequisites
- Kubernetes 1.20+
- kubectl configured
- Ingress controller (nginx recommended)
- cert-manager (for TLS certificates)

### Deployment Steps

1. **Apply the deployment manifests**:
   ```bash
   kubectl apply -f deployment/k8s/distconv-deployment.yaml
   ```

2. **Verify deployment**:
   ```bash
   kubectl get pods -n distconv
   kubectl get services -n distconv
   ```

3. **Access the application**:
   - Update the ingress host in `distconv-deployment.yaml`
   - Access via the configured domain

### Scaling

**Manual scaling**:
```bash
# Scale transcoding engines
kubectl scale deployment transcoding-engine --replicas=5 -n distconv

# Scale dispatch servers
kubectl scale deployment dispatch-server --replicas=3 -n distconv
```

**Auto-scaling**:
The HPA (Horizontal Pod Autoscaler) is configured to automatically scale transcoding engines based on CPU and memory usage.

## GitHub Actions CI/CD

The repository includes comprehensive CI/CD pipelines:

### Workflows

1. **CI Pipeline** (`.github/workflows/ci.yml`):
   - Builds all components with multiple compilers
   - Runs comprehensive tests
   - Performs code quality checks
   - Runs integration tests
   - Security scanning

2. **Release Pipeline** (`.github/workflows/release.yml`):
   - Triggered on version tags
   - Builds release packages
   - Creates GitHub releases
   - Uploads binaries and documentation

3. **Docker Pipeline** (`.github/workflows/docker.yml`):
   - Builds multi-platform Docker images
   - Pushes to GitHub Container Registry
   - Deploys to staging/production environments

### Configuration

1. **Enable GitHub Packages**:
   - Go to repository settings
   - Enable GitHub Container Registry
   - Configure secrets if needed

2. **Set up environments**:
   - Create `staging` and `production` environments
   - Configure environment-specific secrets
   - Set up deployment protection rules

## Environment Variables

### Dispatch Server
- `API_KEY`: API key for authentication
- `LOG_LEVEL`: Logging level (DEBUG, INFO, WARN, ERROR)
- `STATE_FILE`: Path to persistent state file
- `PORT`: Server port (default: 8080)

### Transcoding Engine
- `DISPATCH_SERVER_URL`: URL of the dispatch server
- `TEMP_DIR`: Directory for temporary files
- `LOG_LEVEL`: Logging level
- `ENGINE_ID`: Unique identifier for the engine
- `MAX_CONCURRENT_JOBS`: Maximum concurrent transcoding jobs

### Submission Client
- `DISPATCH_SERVER_URL`: URL of the dispatch server
- `QT_QPA_PLATFORM`: Qt platform (xcb for X11)
- `DISPLAY`: X11 display (for GUI)

## Storage

### Docker Volumes
- `dispatch_data`: Persistent storage for dispatch server
- `dispatch_logs`: Log files for dispatch server
- `transcoding_temp`: Temporary files for transcoding
- `transcoding_logs`: Log files for transcoding engines
- `client_data`: User data for submission client
- `client_config`: Configuration for submission client

### Kubernetes Persistent Volumes
- `dispatch-server-data`: 10Gi for dispatch server data
- `dispatch-server-logs`: 5Gi for dispatch server logs
- `transcoding-engine-logs`: 10Gi for transcoding engine logs (shared)

## Monitoring

### Health Checks
- **Dispatch Server**: HTTP GET to `/api/v1/status`
- **Transcoding Engine**: Process check
- **Submission Client**: Process check

### Metrics
The system supports OpenTelemetry metrics collection:
- Job processing rates
- Engine utilization
- Error rates
- Response times

### Logging
All components use structured logging with configurable levels:
- JSON format for production
- Human-readable format for development
- Centralized log aggregation supported

## Security

### Container Security
- Non-root users in all containers
- Minimal base images
- Security scanning with Trivy
- Regular dependency updates

### Network Security
- Internal communication over private networks
- TLS/SSL for external connections
- API key authentication
- Rate limiting

### Secrets Management
- Environment variables for sensitive data
- Kubernetes secrets for cluster deployment
- Docker secrets for Swarm deployment

## Troubleshooting

### Common Issues

1. **Container won't start**:
   ```bash
   docker logs <container-name>
   kubectl logs <pod-name> -n distconv
   ```

2. **Service connectivity**:
   ```bash
   # Test dispatch server
   curl http://localhost:8080/api/v1/status
   
   # Test from inside container
   docker exec -it <container> curl http://dispatch-server:8080/api/v1/status
   ```

3. **X11 forwarding for submission client**:
   ```bash
   # Enable X11 forwarding
   xhost +local:docker
   
   # Or use VNC for remote access
   docker run -p 5900:5900 -e VNC_PASSWORD=password distconv/submission-client
   ```

### Performance Tuning

1. **Resource limits**:
   - Adjust CPU/memory limits based on workload
   - Monitor resource usage
   - Use node affinity for optimal placement

2. **Scaling**:
   - Scale transcoding engines based on job queue
   - Use HPA for automatic scaling
   - Consider cluster autoscaling for large workloads

3. **Storage**:
   - Use fast storage for temporary files
   - Implement cleanup policies
   - Consider distributed storage for large files

## Development

### Local Development
Use Docker Compose for local development:
```bash
cd deployment/docker
docker-compose up -d
```

### Testing
Run the test suite:
```bash
# Unit tests
cd dispatch_server_cpp && cmake --build build && ./build/dispatch_server_tests

# Integration tests
docker-compose -f deployment/docker/docker-compose.yml up -d
# Run integration test scripts
```

### Debugging
Enable debug logging:
```bash
# Docker
docker run -e LOG_LEVEL=DEBUG distconv/dispatch-server

# Kubernetes
kubectl set env deployment/dispatch-server LOG_LEVEL=DEBUG -n distconv
```