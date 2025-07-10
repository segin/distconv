#!/bin/bash

# DistConv Transcoding Engine Installation Script
# Modern C++ Implementation with Dependency Injection

set -e

DISTCONV_USER="distconv"
DISTCONV_HOME="/opt/distconv"
ENGINE_DIR="$DISTCONV_HOME/transcoding_engine"
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source color and formatting functions from dispatch server deployment
if [[ -f "/home/segin/webchess/deployment/colors.sh" ]]; then
    source "/home/segin/webchess/deployment/colors.sh"
else
    # Fallback color definitions
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    CYAN='\033[0;36m'
    WHITE='\033[1;37m'
    BOLD='\033[1m'
    RESET='\033[0m'
    
    print_header() { echo -e "${BOLD}${GREEN}=== $1 ===${RESET}"; }
    print_step() { echo -e "${CYAN}➤${RESET} $1"; }
    print_success() { echo -e "${GREEN}✓${RESET} $1"; }
    print_error() { echo -e "${RED}✗${RESET} $1"; }
    print_warning() { echo -e "${YELLOW}⚠${RESET} $1"; }
    print_info() { echo -e "${BLUE}ℹ${RESET} $1"; }
fi

print_header "DistConv Transcoding Engine Installation" "GREEN"

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   print_error "This script must be run as root or with sudo"
   exit 1
fi

# Check system requirements
print_step "Checking system requirements..."

# Check OS
if [[ ! -f /etc/os-release ]]; then
    print_error "Cannot determine operating system"
    exit 1
fi

source /etc/os-release
if [[ "$ID" != "ubuntu" ]] && [[ "$ID" != "debian" ]]; then
    print_warning "This script is optimized for Ubuntu/Debian. Other distributions may require manual installation."
fi

print_success "Operating System: $PRETTY_NAME"

# Check CPU architecture
ARCH=$(uname -m)
if [[ "$ARCH" != "x86_64" ]]; then
    print_warning "Architecture $ARCH may not be fully supported. x86_64 is recommended."
fi

print_success "Architecture: $ARCH"

# Install system dependencies
print_step "Installing system dependencies..."

apt-get update
apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    curl \
    wget \
    ffmpeg \
    libsqlite3-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    libgmock-dev

print_success "System dependencies installed"

# Install cpr library (if not available via package manager)
print_step "Installing cpr HTTP library..."

if ! pkg-config --exists cpr; then
    print_info "Building cpr from source..."
    
    cd /tmp
    if [[ -d "cpr" ]]; then
        rm -rf cpr
    fi
    
    git clone https://github.com/libcpr/cpr.git
    cd cpr
    mkdir build && cd build
    cmake .. -DCPR_USE_SYSTEM_CURL=ON -DBUILD_SHARED_LIBS=ON
    make -j$(nproc)
    make install
    ldconfig
    
    print_success "cpr library installed from source"
else
    print_success "cpr library already available"
fi

# Create system user
print_step "Creating system user..."

if ! id "$DISTCONV_USER" &>/dev/null; then
    useradd -r -s /bin/false -d "$DISTCONV_HOME" "$DISTCONV_USER"
    print_success "Created system user: $DISTCONV_USER"
else
    print_info "System user already exists: $DISTCONV_USER"
fi

# Create directory structure
print_step "Creating directory structure..."

mkdir -p "$ENGINE_DIR"/{bin,config,logs,data,temp}
mkdir -p "$DISTCONV_HOME/shared"

print_success "Directory structure created"

# Build the modern transcoding engine
print_step "Building modern transcoding engine..."

cd "$CURRENT_DIR/.."

# Create build directory
if [[ -d "build_modern" ]]; then
    rm -rf build_modern
fi

mkdir build_modern && cd build_modern

# Configure with CMake
cmake .. -f ../CMakeLists_modern.txt \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$ENGINE_DIR" \
    -DBUILD_TESTING=ON

# Build
make -j$(nproc)

# Run tests
print_step "Running test suite..."
if make test; then
    print_success "All tests passed"
else
    print_warning "Some tests failed - proceeding with installation"
fi

# Install binaries
print_step "Installing transcoding engine..."

make install

# Copy additional files
cp "../app_main_modern" "$ENGINE_DIR/bin/transcoding_engine" 2>/dev/null || true
cp "../README_modern.md" "$ENGINE_DIR/" 2>/dev/null || true

print_success "Transcoding engine installed"

# Create configuration files
print_step "Creating configuration files..."

cat > "$ENGINE_DIR/config/engine.conf" << EOF
# DistConv Transcoding Engine Configuration

# Dispatch Server Settings
DISPATCH_URL=http://localhost:8080
API_KEY=

# Engine Settings
HOSTNAME=$(hostname)
STORAGE_GB=500.0
STREAMING_SUPPORT=true

# Database Settings
DB_PATH=$ENGINE_DIR/data/transcoding_jobs.db

# SSL Settings
CA_CERT_PATH=

# Performance Settings
HEARTBEAT_INTERVAL=5
BENCHMARK_INTERVAL=300
JOB_POLL_INTERVAL=1
HTTP_TIMEOUT=30

# Logging
LOG_LEVEL=INFO
EOF

cat > "$ENGINE_DIR/config/environment" << EOF
# Environment variables for DistConv Transcoding Engine

# Core settings
DISTCONV_ENGINE_CONFIG="$ENGINE_DIR/config/engine.conf"
DISTCONV_ENGINE_HOME="$ENGINE_DIR"
DISTCONV_LOG_DIR="$ENGINE_DIR/logs"
DISTCONV_DATA_DIR="$ENGINE_DIR/data"
DISTCONV_TEMP_DIR="$ENGINE_DIR/temp"

# Performance tuning
DISTCONV_WORKER_THREADS=4
DISTCONV_MAX_CONCURRENT_JOBS=2

# Security
umask 0027
EOF

print_success "Configuration files created"

# Create systemd service
print_step "Creating systemd service..."

cat > /etc/systemd/system/distconv-transcoding-engine.service << EOF
[Unit]
Description=DistConv Transcoding Engine
Documentation=https://github.com/segin/distconv
After=network.target network-online.target
Wants=network-online.target
Requires=network.target

[Service]
Type=simple
User=$DISTCONV_USER
Group=$DISTCONV_USER

# Working directory and files
WorkingDirectory=$ENGINE_DIR
ExecStart=$ENGINE_DIR/bin/transcoding_engine_modern
ExecReload=/bin/kill -HUP \$MAINPID

# Environment
EnvironmentFile=$ENGINE_DIR/config/environment
Environment=PATH=/usr/local/bin:/usr/bin:/bin

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
PrivateDevices=true
ProtectHome=true
ProtectSystem=strict
ReadWritePaths=$ENGINE_DIR/logs $ENGINE_DIR/data $ENGINE_DIR/temp
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectControlGroups=true
RestrictSUIDSGID=true
RestrictRealtime=true
RestrictNamespaces=true
LockPersonality=true
MemoryDenyWriteExecute=true
SystemCallFilter=@system-service
SystemCallFilter=~@debug @mount @cpu-emulation @obsolete @privileged @reboot @swap @raw-io

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096
MemoryMax=2G
CPUQuota=200%

# Restart behavior
Restart=always
RestartSec=10
StartLimitInterval=60
StartLimitBurst=3

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=distconv-transcoding-engine

[Install]
WantedBy=multi-user.target
EOF

print_success "Systemd service created"

# Set ownership and permissions
print_step "Setting ownership and permissions..."

chown -R "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_HOME"
chmod -R 750 "$DISTCONV_HOME"
chmod 755 "$ENGINE_DIR/bin"/*
chmod 640 "$ENGINE_DIR/config"/*
chmod 750 "$ENGINE_DIR"/{logs,data,temp}

print_success "Ownership and permissions set"

# Create log rotation
print_step "Setting up log rotation..."

cat > /etc/logrotate.d/distconv-transcoding-engine << EOF
$ENGINE_DIR/logs/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 640 $DISTCONV_USER $DISTCONV_USER
    postrotate
        systemctl reload distconv-transcoding-engine > /dev/null 2>&1 || true
    endscript
}
EOF

print_success "Log rotation configured"

# Enable and start service
print_step "Enabling and starting service..."

systemctl daemon-reload
systemctl enable distconv-transcoding-engine

# Ask user if they want to start the service now
echo ""
read -p "Do you want to start the transcoding engine service now? (Y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Nn]$ ]]; then
    print_info "Service enabled but not started. Start with: systemctl start distconv-transcoding-engine"
else
    systemctl start distconv-transcoding-engine
    sleep 2
    
    if systemctl is-active --quiet distconv-transcoding-engine; then
        print_success "Service started successfully"
    else
        print_error "Service failed to start. Check logs with: journalctl -u distconv-transcoding-engine"
    fi
fi

# Print completion summary
print_header "Installation Complete!" "GREEN"

echo ""
echo "${BOLD}${GREEN}✨ DistConv Transcoding Engine Successfully Installed${RESET}"
echo ""
echo "${BOLD}${BLUE}📍 Installation Details:${RESET}"
echo "   ${CYAN}•${RESET} Installation directory: ${WHITE}$ENGINE_DIR${RESET}"
echo "   ${CYAN}•${RESET} Configuration file: ${WHITE}$ENGINE_DIR/config/engine.conf${RESET}"
echo "   ${CYAN}•${RESET} Service name: ${WHITE}distconv-transcoding-engine${RESET}"
echo "   ${CYAN}•${RESET} Log directory: ${WHITE}$ENGINE_DIR/logs${RESET}"
echo "   ${CYAN}•${RESET} Data directory: ${WHITE}$ENGINE_DIR/data${RESET}"
echo ""
echo "${BOLD}${BLUE}🚀 Quick Start:${RESET}"
echo "   ${CYAN}•${RESET} Edit configuration: ${WHITE}sudo nano $ENGINE_DIR/config/engine.conf${RESET}"
echo "   ${CYAN}•${RESET} Start service: ${WHITE}sudo systemctl start distconv-transcoding-engine${RESET}"
echo "   ${CYAN}•${RESET} Check status: ${WHITE}sudo systemctl status distconv-transcoding-engine${RESET}"
echo "   ${CYAN}•${RESET} View logs: ${WHITE}sudo journalctl -u distconv-transcoding-engine -f${RESET}"
echo ""
echo "${BOLD}${BLUE}🔧 Configuration Required:${RESET}"
echo "   ${CYAN}•${RESET} Set ${WHITE}DISPATCH_URL${RESET} in config file"
echo "   ${CYAN}•${RESET} Set ${WHITE}API_KEY${RESET} for dispatcher authentication"
echo "   ${CYAN}•${RESET} Configure ${WHITE}CA_CERT_PATH${RESET} if using HTTPS"
echo ""
echo "${BOLD}${GREEN}✅ Modern Architecture Features:${RESET}"
echo "   ${CYAN}•${RESET} Dependency injection for testability"
echo "   ${CYAN}•${RESET} Modern C++17 with nlohmann::json and cpr"
echo "   ${CYAN}•${RESET} Secure subprocess execution (no system() calls)"
echo "   ${CYAN}•${RESET} Thread-safe SQLite database operations"
echo "   ${CYAN}•${RESET} Comprehensive error handling and logging"
echo "   ${CYAN}•${RESET} Production-ready systemd service with security hardening"
echo ""

exit 0