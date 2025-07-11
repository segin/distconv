#!/bin/bash

# DistConv Dispatch Server Installation Script
# Run as root or with sudo privileges

set -e

DISTCONV_USER="distconv"
DISTCONV_HOME="/opt/distconv"
DISTCONV_CONFIG_DIR="/etc/distconv"
DISTCONV_DATA_DIR="/var/lib/distconv"
DISTCONV_LOG_DIR="/var/log/distconv"
DISTCONV_RUN_DIR="/run/distconv"
DISTCONV_CACHE_DIR="/var/cache/distconv"
CONFIG_BACKUP_DIR="/tmp/distconv_config_backup"
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$CURRENT_DIR")"

# Source color and formatting functions
source "$CURRENT_DIR/colors.sh"

print_header "DistConv Dispatch Server Installation"

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   print_error "This script must be run as root or with sudo"
   exit 1
fi

# Function to check system requirements
check_requirements() {
    print_step "Checking system requirements..."
    
    # Check for required commands
    local missing_commands=()
    
    for cmd in cmake make g++ git; do
        if ! command -v "$cmd" &> /dev/null; then
            missing_commands+=("$cmd")
        fi
    done
    
    if [[ ${#missing_commands[@]} -gt 0 ]]; then
        print_error "Missing required commands: ${missing_commands[*]}"
        print_info "Please install build tools: apt update && apt install -y build-essential cmake git"
        exit 1
    fi
    
    print_success "System requirements satisfied"
}

# Function to create FHS-compliant directory structure
create_directories() {
    print_step "Creating FHS-compliant directory structure..."
    
    # Create all necessary directories
    mkdir -p "$DISTCONV_HOME"/{server/bin,engine/bin,lib,share}
    mkdir -p "$DISTCONV_CONFIG_DIR"/{ssl,backup}
    mkdir -p "$DISTCONV_DATA_DIR"/{dispatch,engine,backup}
    mkdir -p "$DISTCONV_LOG_DIR"
    mkdir -p "$DISTCONV_RUN_DIR"/{sockets,locks}
    mkdir -p "$DISTCONV_CACHE_DIR"
    mkdir -p /tmp/distconv
    
    # Set proper ownership
    chown -R "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_HOME"
    chown -R "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_DATA_DIR"
    chown -R "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_LOG_DIR"
    chown -R "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_RUN_DIR"
    chown -R "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_CACHE_DIR"
    chown -R "$DISTCONV_USER:$DISTCONV_USER" /tmp/distconv
    
    # Set proper permissions
    chmod 755 "$DISTCONV_HOME"
    chmod 750 "$DISTCONV_CONFIG_DIR"
    chmod 700 "$DISTCONV_CONFIG_DIR/ssl"
    chmod 750 "$DISTCONV_DATA_DIR"
    chmod 755 "$DISTCONV_LOG_DIR"
    chmod 755 "$DISTCONV_RUN_DIR"
    chmod 755 "$DISTCONV_CACHE_DIR"
    chmod 1777 /tmp/distconv  # Sticky bit for temp directory
    
    print_success "Directory structure created"
}

# Function to backup configuration files
backup_config() {
    if [[ -d "$DISTCONV_HOME" ]] || [[ -d "$DISTCONV_CONFIG_DIR" ]]; then
        print_step "Backing up existing configuration..."
        mkdir -p "$CONFIG_BACKUP_DIR"
        
        # Backup potential config files
        if [[ -f "$DISTCONV_HOME/config/server.conf" ]]; then
            cp "$DISTCONV_HOME/config/server.conf" "$CONFIG_BACKUP_DIR/"
            print_success "Backed up server.conf file"
        fi
        
        if [[ -f "$DISTCONV_HOME/server/state.json" ]]; then
            cp "$DISTCONV_HOME/server/state.json" "$CONFIG_BACKUP_DIR/"
            print_success "Backed up state.json file"
        fi
        
        # Backup entire config directory
        if [[ -d "$DISTCONV_HOME/config" ]]; then
            cp -r "$DISTCONV_HOME/config" "$CONFIG_BACKUP_DIR/"
            print_success "Backed up config directory"
        fi
        
        # Backup logs directory
        if [[ -d "$DISTCONV_HOME/logs" ]] && [[ "$(ls -A "$DISTCONV_HOME/logs")" ]]; then
            cp -r "$DISTCONV_HOME/logs" "$CONFIG_BACKUP_DIR/"
            print_success "Backed up logs directory"
        fi
    fi
}

# Function to restore configuration files
restore_config() {
    if [[ -d "$CONFIG_BACKUP_DIR" ]]; then
        print_step "Restoring configuration files..."
        
        if [[ -f "$CONFIG_BACKUP_DIR/server.conf" ]]; then
            cp "$CONFIG_BACKUP_DIR/server.conf" "$DISTCONV_HOME/config/"
            chown "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_HOME/config/server.conf"
            print_success "Restored server.conf file"
        fi
        
        if [[ -f "$CONFIG_BACKUP_DIR/state.json" ]]; then
            cp "$CONFIG_BACKUP_DIR/state.json" "$DISTCONV_HOME/server/"
            chown "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_HOME/server/state.json"
            print_success "Restored state.json file"
        fi
        
        if [[ -d "$CONFIG_BACKUP_DIR/config" ]]; then
            cp -r "$CONFIG_BACKUP_DIR/config"/* "$DISTCONV_HOME/config/" 2>/dev/null || true
            chown -R "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_HOME/config"
            print_success "Restored config directory"
        fi
        
        if [[ -d "$CONFIG_BACKUP_DIR/logs" ]]; then
            cp -r "$CONFIG_BACKUP_DIR/logs"/* "$DISTCONV_HOME/logs/" 2>/dev/null || true
            chown -R "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_HOME/logs"
            print_success "Restored logs directory"
        fi
        
        # Clean up backup
        rm -rf "$CONFIG_BACKUP_DIR"
    fi
}

# Check system requirements
check_requirements

# Handle existing installation
if [[ -d "$DISTCONV_HOME" ]]; then
    print_warning "Existing DistConv installation detected"
    
    # Stop service if running
    if systemctl is-active --quiet distconv-dispatch 2>/dev/null; then
        print_step "Stopping DistConv service..."
        systemctl stop distconv-dispatch
    fi
    
    # Backup configuration
    backup_config
    
    # Remove old installation (but keep user)
    print_step "Removing old installation..."
    rm -rf "$DISTCONV_HOME"
fi

# Install system dependencies
print_step "Installing system dependencies..."
apt update
apt install -y build-essential cmake git curl wget openssl

# Create system user
print_step "Creating system user: $DISTCONV_USER"
if ! id "$DISTCONV_USER" &>/dev/null; then
    useradd --system --shell /bin/false --home "$DISTCONV_HOME" "$DISTCONV_USER"
    print_success "User $DISTCONV_USER created"
else
    print_info "User $DISTCONV_USER already exists"
fi

# Create FHS-compliant directory structure
create_directories

# Build the project
print_step "Building DistConv Dispatch Server..."
cd "$PROJECT_ROOT/dispatch_server_cpp"

# Clean previous build
if [[ -d "build" ]]; then
    rm -rf build
fi

mkdir build
cd build

# Configure and build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8

# Verify build succeeded
if [[ ! -f "dispatch_server_app" ]]; then
    print_error "Build failed - dispatch_server_app not found"
    exit 1
fi

print_success "Build completed successfully"

# Install binaries
print_step "Installing binaries..."
cp dispatch_server_app "$DISTCONV_HOME/server/bin/"
cp dispatch_server_modern "$DISTCONV_HOME/server/bin/"
cp ../dispatch_server_core.h "$DISTCONV_HOME/server/"
chmod +x "$DISTCONV_HOME/server/bin/dispatch_server_app"
chmod +x "$DISTCONV_HOME/server/bin/dispatch_server_modern"

# Install FHS-compliant configuration files
install_config_files() {
    print_step "Installing configuration files..."
    
    # Install configuration templates
    cp "$CURRENT_DIR/etc/distconv/"* "$DISTCONV_CONFIG_DIR/"
    
    # Set proper permissions for sensitive files
    chmod 600 "$DISTCONV_CONFIG_DIR/api-keys.conf"
    chmod 640 "$DISTCONV_CONFIG_DIR/dispatch-server.conf"
    chmod 640 "$DISTCONV_CONFIG_DIR/transcoding-engine.conf"
    chmod 644 "$DISTCONV_CONFIG_DIR/ports.conf"
    chmod 644 "$DISTCONV_CONFIG_DIR/directories.conf"
    
    # Generate secure API key if not already configured
    if grep -q "CHANGE-THIS" "$DISTCONV_CONFIG_DIR/api-keys.conf"; then
        print_step "Generating secure API keys..."
        local api_key=$(openssl rand -hex 32)
        sed -i "s/CHANGE-THIS-TO-A-SECURE-RANDOM-KEY/$api_key/g" "$DISTCONV_CONFIG_DIR/api-keys.conf"
        sed -i "s/change-this-production-api-key/$api_key/g" "$DISTCONV_CONFIG_DIR/dispatch-server.conf"
        print_success "Generated secure API key: ${api_key:0:8}..."
    fi
    
    chown -R root:$DISTCONV_USER "$DISTCONV_CONFIG_DIR"
    print_success "Configuration files installed"
}

install_config_files

# Create legacy configuration if it doesn't exist (backward compatibility)
if [[ ! -f "$DISTCONV_HOME/config/server.conf" ]]; then
    print_step "Creating legacy configuration for backward compatibility..."
    mkdir -p "$DISTCONV_HOME/config"
    cat > "$DISTCONV_HOME/config/server.conf" << EOF
# DistConv Dispatch Server Configuration
# Generated on $(date)

# API Key for authentication (REQUIRED - change this!)
DISTCONV_API_KEY=$(openssl rand -hex 32)

# Server port (default: 8080)
DISTCONV_PORT=8080

# State file location
DISTCONV_STATE_FILE=$DISTCONV_HOME/server/state.json

# Log level (DEBUG, INFO, WARN, ERROR)
DISTCONV_LOG_LEVEL=INFO
EOF
    print_success "Generated secure default configuration"
else
    print_info "Existing configuration file preserved"
fi

# Set proper ownership
chown -R "$DISTCONV_USER:$DISTCONV_USER" "$DISTCONV_HOME"
chmod 644 "$DISTCONV_HOME/config/server.conf"

# Restore configuration files
restore_config

# Install systemd service
print_step "Installing systemd service..."
cp "$CURRENT_DIR/distconv-dispatch.service" /etc/systemd/system/
cp "$CURRENT_DIR/distconv-transcoding-engine.service" /etc/systemd/system/
systemctl daemon-reload

# Create logrotate configuration
print_step "Setting up log rotation..."
cat > /etc/logrotate.d/distconv << EOF
$DISTCONV_LOG_DIR/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 0644 $DISTCONV_USER $DISTCONV_USER
    postrotate
        systemctl reload distconv-dispatch > /dev/null 2>&1 || true
        systemctl reload distconv-transcoding-engine > /dev/null 2>&1 || true
    endscript
}
EOF

# Enable and start services
print_step "Enabling and starting DistConv services..."
systemctl enable distconv-dispatch
systemctl enable distconv-transcoding-engine
systemctl start distconv-dispatch
systemctl start distconv-transcoding-engine

# Check service status
print_step "Checking service status..."
sleep 2
if systemctl is-active --quiet distconv-dispatch; then
    print_success "DistConv Dispatch Server is running successfully"
else
    print_error "DistConv Dispatch Server failed to start"
    print_error "Check logs with: journalctl -u distconv-dispatch -f"
fi

if systemctl is-active --quiet distconv-transcoding-engine; then
    print_success "DistConv Transcoding Engine is running successfully"
else
    print_warning "DistConv Transcoding Engine status unclear - this is normal if no engine binary is installed"
fi

if systemctl is-active --quiet distconv-dispatch; then
    print_success "Services will start automatically on boot"
else
    exit 1
fi

# Test API endpoint
print_step "Testing API endpoint..."
sleep 1
if curl -s http://localhost:8080/ | grep -q "OK"; then
    print_success "API endpoint is responding correctly"
else
    print_warning "API endpoint may not be fully ready yet"
    print_info "This is normal - the service may need a few more seconds to start"
fi

print_completion_box "Installation Complete!"

echo "${BOLD}${YELLOW}Service Management Commands:${RESET}"
echo "  ${CYAN}Start:${RESET}   ${WHITE}systemctl start distconv-dispatch${RESET}"
echo "  ${CYAN}Stop:${RESET}    ${WHITE}systemctl stop distconv-dispatch${RESET}"
echo "  ${CYAN}Restart:${RESET} ${WHITE}systemctl restart distconv-dispatch${RESET}"
echo "  ${CYAN}Status:${RESET}  ${WHITE}systemctl status distconv-dispatch${RESET}"
echo "  ${CYAN}Logs:${RESET}    ${WHITE}journalctl -u distconv-dispatch -f${RESET}"
echo ""

echo "${BOLD}${YELLOW}Configuration:${RESET}"
echo "  ${CYAN}Install Dir:${RESET} ${WHITE}$DISTCONV_HOME${RESET}"
echo "  ${CYAN}Config Dir:${RESET}  ${WHITE}$DISTCONV_CONFIG_DIR${RESET}"
echo "  ${CYAN}Data Dir:${RESET}    ${WHITE}$DISTCONV_DATA_DIR${RESET}"
echo "  ${CYAN}Log Dir:${RESET}     ${WHITE}$DISTCONV_LOG_DIR${RESET}"
echo "  ${CYAN}Legacy Config:${RESET} ${WHITE}$DISTCONV_HOME/config/server.conf${RESET}"
echo "  ${CYAN}Services:${RESET}    ${WHITE}/etc/systemd/system/distconv-*.service${RESET}"
echo "  ${CYAN}User:${RESET}       ${WHITE}$DISTCONV_USER${RESET}"
echo ""

echo "${BOLD}${MAGENTA}🎯 API is accessible at: ${WHITE}http://localhost:8080${RESET}"
echo ""

# Display API key information
if [[ -f "$DISTCONV_CONFIG_DIR/api-keys.conf" ]]; then
    API_KEY=$(grep "DISTCONV_API_KEY=" "$DISTCONV_CONFIG_DIR/api-keys.conf" | cut -d'=' -f2)
    if [[ -n "$API_KEY" ]]; then
        echo "${BOLD}${YELLOW}🔑 Your API Key:${RESET} ${WHITE}$API_KEY${RESET}"
        echo ""
        echo "${BOLD}${CYAN}Test the API:${RESET}"
        echo "${WHITE}curl -H \"X-API-Key: $API_KEY\" http://localhost:8080/jobs/${RESET}"
        echo ""
    fi
elif [[ -f "$DISTCONV_HOME/config/server.conf" ]]; then
    API_KEY=$(grep "DISTCONV_API_KEY=" "$DISTCONV_HOME/config/server.conf" | cut -d'=' -f2)
    if [[ -n "$API_KEY" ]]; then
        echo "${BOLD}${YELLOW}🔑 Your API Key (Legacy):${RESET} ${WHITE}$API_KEY${RESET}"
        echo ""
        echo "${BOLD}${CYAN}Test the API:${RESET}"
        echo "${WHITE}curl -H \"X-API-Key: $API_KEY\" http://localhost:8080/jobs/${RESET}"
        echo ""
    fi
fi

echo "${BOLD}${YELLOW}🌐 Setting up Reverse Proxy (Optional):${RESET}"
echo "${BOLD}1.${RESET} Install nginx: ${WHITE}apt install nginx${RESET}"
echo "${BOLD}2.${RESET} Configure nginx for HTTPS and domain access"
echo "${BOLD}3.${RESET} Update firewall rules to allow HTTP/HTTPS traffic"
echo ""

echo "${BOLD}${BLUE}📚 Documentation:${RESET}"
echo "   ${CYAN}•${RESET} API Reference: ${WHITE}https://github.com/segin/distconv/blob/main/docs/protocol.md${RESET}"
echo "   ${CYAN}•${RESET} User Guide: ${WHITE}https://github.com/segin/distconv/blob/main/README.md${RESET}"
echo "   ${CYAN}•${RESET} Configuration: ${WHITE}$DISTCONV_HOME/config/server.conf.example${RESET}"
echo ""

echo "${BOLD}${BLUE}💡 To upgrade DistConv in the future, simply run this script again.${RESET}"
echo "${BOLD}${BLUE}   Your configuration files and job state will be preserved automatically.${RESET}"
echo ""

echo "${BOLD}${GREEN}✨ DistConv Dispatch Server installation completed successfully!${RESET}"