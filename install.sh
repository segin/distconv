#!/bin/bash

# DistConv Dispatch Server Installation Script
# This script installs the DistConv dispatch server to /opt/distconv/server
# and sets up a systemd service for production deployment

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   print_error "This script must be run as root (use sudo)"
   exit 1
fi

print_status "Starting DistConv Dispatch Server installation..."

# Install dependencies
print_status "Installing system dependencies..."
apt update
apt install -y build-essential cmake git curl wget

# Create distconv user if it doesn't exist
if ! id "distconv" &>/dev/null; then
    print_status "Creating distconv system user..."
    useradd -r -s /bin/false -d /opt/distconv distconv
else
    print_warning "User 'distconv' already exists"
fi

# Create directory structure
print_status "Creating directory structure..."
mkdir -p /opt/distconv/server
mkdir -p /opt/distconv/logs
mkdir -p /opt/distconv/config

# Check if we're in the git repository
if [ -d "dispatch_server_cpp" ]; then
    # We're in the repository directory
    REPO_DIR="$(pwd)"
    print_status "Using current directory as repository: $REPO_DIR"
else
    # Clone the repository
    print_status "Cloning DistConv repository..."
    cd /tmp
    if [ -d "distconv" ]; then
        rm -rf distconv
    fi
    git clone https://github.com/segin/distconv.git
    REPO_DIR="/tmp/distconv"
fi

# Build the project
print_status "Building dispatch server..."
cd "$REPO_DIR/dispatch_server_cpp"

# Clean previous build
if [ -d "build" ]; then
    rm -rf build
fi

mkdir build
cd build

# Configure and build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Verify build succeeded
if [ ! -f "dispatch_server_app" ]; then
    print_error "Build failed - dispatch_server_app not found"
    exit 1
fi

print_status "Build completed successfully"

# Install binaries
print_status "Installing binaries to /opt/distconv/server..."
cp dispatch_server_app /opt/distconv/server/
cp ../dispatch_server_core.h /opt/distconv/server/

# Make binary executable
chmod +x /opt/distconv/server/dispatch_server_app

# Create default configuration
print_status "Creating default configuration..."
cat > /opt/distconv/config/server.conf << EOF
# DistConv Dispatch Server Configuration
# Environment variables for the dispatch server

# API Key for authentication (REQUIRED - change this!)
DISTCONV_API_KEY=change_this_secure_api_key_$(openssl rand -hex 16)

# Server port (default: 8080)
DISTCONV_PORT=8080

# State file location
DISTCONV_STATE_FILE=/opt/distconv/server/state.json

# Log level (DEBUG, INFO, WARN, ERROR)
DISTCONV_LOG_LEVEL=INFO
EOF

# Create systemd service file
print_status "Creating systemd service..."
cat > /etc/systemd/system/distconv-dispatch.service << EOF
[Unit]
Description=DistConv Dispatch Server
Documentation=https://github.com/segin/distconv
After=network.target
Wants=network.target

[Service]
Type=simple
User=distconv
Group=distconv
WorkingDirectory=/opt/distconv/server

# Load configuration from file
EnvironmentFile=/opt/distconv/config/server.conf

# Start the server
ExecStart=/opt/distconv/server/dispatch_server_app --api-key \${DISTCONV_API_KEY}

# Service management
Restart=always
RestartSec=10
StartLimitInterval=60
StartLimitBurst=3

# Security settings
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/opt/distconv
CapabilityBoundingSet=
AmbientCapabilities=
SystemCallFilter=@system-service
SystemCallErrorNumber=EPERM

# Resource limits
LimitNOFILE=65536
MemoryLimit=1G

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=distconv-dispatch

[Install]
WantedBy=multi-user.target
EOF

# Set proper ownership
print_status "Setting file permissions..."
chown -R distconv:distconv /opt/distconv
chmod 755 /opt/distconv/server/dispatch_server_app
chmod 644 /opt/distconv/config/server.conf
chmod 755 /opt/distconv/logs

# Create logrotate configuration
print_status "Setting up log rotation..."
cat > /etc/logrotate.d/distconv << EOF
/opt/distconv/logs/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 0644 distconv distconv
    postrotate
        systemctl reload distconv-dispatch > /dev/null 2>&1 || true
    endscript
}
EOF

# Reload systemd
print_status "Reloading systemd configuration..."
systemctl daemon-reload

# Enable service but don't start it yet
systemctl enable distconv-dispatch

print_status "Installation completed successfully!"
echo
print_warning "IMPORTANT: Before starting the service, please:"
print_warning "1. Edit /opt/distconv/config/server.conf to set a secure API key"
print_warning "2. Review the configuration settings"
print_warning "3. Start the service with: sudo systemctl start distconv-dispatch"
echo

# Show service status
print_status "Service management commands:"
echo "  Start service:    sudo systemctl start distconv-dispatch"
echo "  Stop service:     sudo systemctl stop distconv-dispatch" 
echo "  Restart service:  sudo systemctl restart distconv-dispatch"
echo "  Check status:     sudo systemctl status distconv-dispatch"
echo "  View logs:        sudo journalctl -u distconv-dispatch -f"
echo

# Show API endpoint
print_status "Once started, the API will be available at:"
echo "  Health check:     curl http://localhost:8080/"
echo "  API endpoints:    curl -H \"X-API-Key: YOUR_KEY\" http://localhost:8080/jobs/"
echo

# Show next steps
print_status "Next steps:"
echo "1. Configure your API key:"
echo "   sudo nano /opt/distconv/config/server.conf"
echo
echo "2. Start the service:"
echo "   sudo systemctl start distconv-dispatch"
echo
echo "3. Check the service is running:"
echo "   sudo systemctl status distconv-dispatch"
echo "   curl http://localhost:8080/"
echo
echo "4. View documentation:"
echo "   https://github.com/segin/distconv/blob/main/README.md"
echo "   https://github.com/segin/distconv/blob/main/docs/protocol.md"

print_status "Installation script completed!"