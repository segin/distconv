#!/bin/bash

# DistConv Transcoding Engine Uninstall Script
# Modern C++ Implementation

set -e

DISTCONV_USER="distconv"
DISTCONV_HOME="/opt/distconv"
ENGINE_DIR="$DISTCONV_HOME/transcoding_engine"
BACKUP_DIR="$HOME/distconv_transcoding_backup_$(date +%Y%m%d_%H%M%S)"

# Source color functions
if [[ -f "/home/segin/webchess/deployment/colors.sh" ]]; then
    source "/home/segin/webchess/deployment/colors.sh"
else
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

print_header "DistConv Transcoding Engine Uninstall" "RED"

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   print_error "This script must be run as root or with sudo"
   exit 1
fi

# Confirmation prompt
echo "${BOLD}${RED}⚠ Warning: This will completely remove the DistConv Transcoding Engine!${RESET}"
echo ""
echo "${BOLD}${YELLOW}This includes:${RESET}"
echo "   ${CYAN}•${RESET} Application files in ${WHITE}$ENGINE_DIR${RESET}"
echo "   ${CYAN}•${RESET} Systemd service configuration"
echo "   ${CYAN}•${RESET} Log rotation configuration"
echo "   ${CYAN}•${RESET} All job data and configuration"
echo ""
read -p "${BOLD}Are you sure you want to uninstall the Transcoding Engine? (y/N): ${RESET}" -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    print_info "Uninstall cancelled."
    exit 0
fi

# Ask about backup
echo ""
read -p "${BOLD}Do you want to backup configuration and data before uninstalling? (Y/n): ${RESET}" -n 1 -r
echo
if [[ $REPLY =~ ^[Nn]$ ]]; then
    BACKUP_CONFIG=false
else
    BACKUP_CONFIG=true
fi

# Stop and disable service
if systemctl is-active --quiet distconv-transcoding-engine 2>/dev/null; then
    print_step "Stopping transcoding engine service..."
    systemctl stop distconv-transcoding-engine
fi

if systemctl is-enabled --quiet distconv-transcoding-engine 2>/dev/null; then
    print_step "Disabling transcoding engine service..."
    systemctl disable distconv-transcoding-engine
fi

# Backup configuration if requested
if [[ $BACKUP_CONFIG == true ]] && [[ -d "$ENGINE_DIR" ]]; then
    print_step "Backing up configuration to: $BACKUP_DIR"
    mkdir -p "$BACKUP_DIR"
    
    # Backup config files
    if [[ -d "$ENGINE_DIR/config" ]]; then
        cp -r "$ENGINE_DIR/config" "$BACKUP_DIR/"
        print_success "Backed up configuration files"
    fi
    
    # Backup database
    if [[ -f "$ENGINE_DIR/data/transcoding_jobs.db" ]]; then
        mkdir -p "$BACKUP_DIR/data"
        cp "$ENGINE_DIR/data/transcoding_jobs.db" "$BACKUP_DIR/data/"
        print_success "Backed up job database"
    fi
    
    # Backup logs (last 7 days)
    if [[ -d "$ENGINE_DIR/logs" ]] && [[ "$(ls -A "$ENGINE_DIR/logs" 2>/dev/null)" ]]; then
        mkdir -p "$BACKUP_DIR/logs"
        find "$ENGINE_DIR/logs" -name "*.log*" -mtime -7 -exec cp {} "$BACKUP_DIR/logs/" \; 2>/dev/null || true
        print_success "Backed up recent logs"
    fi
    
    # Create restoration script
    cat > "$BACKUP_DIR/restore.sh" << 'EOF'
#!/bin/bash
# DistConv Transcoding Engine Configuration Restoration Script

set -e

ENGINE_DIR="/opt/distconv/transcoding_engine"
BACKUP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root or with sudo"
   exit 1
fi

echo "Restoring DistConv Transcoding Engine configuration from backup..."

# Stop service if running
if systemctl is-active --quiet distconv-transcoding-engine 2>/dev/null; then
    systemctl stop distconv-transcoding-engine
fi

# Restore configuration files
if [[ -d "$BACKUP_DIR/config" ]]; then
    mkdir -p "$ENGINE_DIR/config"
    cp -r "$BACKUP_DIR/config"/* "$ENGINE_DIR/config/"
    chown -R distconv:distconv "$ENGINE_DIR/config"
    echo "✓ Restored configuration files"
fi

# Restore database
if [[ -f "$BACKUP_DIR/data/transcoding_jobs.db" ]]; then
    mkdir -p "$ENGINE_DIR/data"
    cp "$BACKUP_DIR/data/transcoding_jobs.db" "$ENGINE_DIR/data/"
    chown -R distconv:distconv "$ENGINE_DIR/data"
    echo "✓ Restored job database"
fi

# Start service
systemctl start distconv-transcoding-engine
echo "✓ Transcoding Engine service restarted"

echo ""
echo "Configuration restoration completed!"
echo "Your DistConv Transcoding Engine should now have all your previous settings."
EOF
    chmod +x "$BACKUP_DIR/restore.sh"
fi

# Remove systemd service file
if [[ -f "/etc/systemd/system/distconv-transcoding-engine.service" ]]; then
    print_step "Removing systemd service file..."
    rm -f /etc/systemd/system/distconv-transcoding-engine.service
    systemctl daemon-reload
fi

# Remove logrotate configuration
if [[ -f "/etc/logrotate.d/distconv-transcoding-engine" ]]; then
    print_step "Removing log rotation configuration..."
    rm -f /etc/logrotate.d/distconv-transcoding-engine
fi

# Remove transcoding engine directory
if [[ -d "$ENGINE_DIR" ]]; then
    print_step "Removing transcoding engine directory: $ENGINE_DIR"
    rm -rf "$ENGINE_DIR"
fi

# Ask about removing shared directory
if [[ -d "$DISTCONV_HOME/shared" ]]; then
    echo ""
    read -p "${BOLD}Remove shared DistConv directory? (y/N): ${RESET}" -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$DISTCONV_HOME/shared"
        print_success "Removed shared directory"
    fi
fi

# Ask about removing main directory if empty
if [[ -d "$DISTCONV_HOME" ]] && [[ -z "$(ls -A "$DISTCONV_HOME" 2>/dev/null)" ]]; then
    print_step "Removing empty DistConv directory: $DISTCONV_HOME"
    rmdir "$DISTCONV_HOME"
fi

# Ask about removing user (only if dispatch server is not installed)
if ! systemctl is-enabled --quiet distconv-dispatch 2>/dev/null; then
    echo ""
    read -p "${BOLD}Remove system user '$DISTCONV_USER'? (y/N): ${RESET}" -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if id "$DISTCONV_USER" &>/dev/null; then
            print_step "Removing system user: $DISTCONV_USER"
            userdel "$DISTCONV_USER"
            print_success "System user removed"
        fi
    else
        print_info "System user '$DISTCONV_USER' preserved (may be used by dispatch server)"
    fi
else
    print_info "System user '$DISTCONV_USER' preserved (used by dispatch server)"
fi

print_header "Uninstall Complete!" "GREEN"

if [[ $BACKUP_CONFIG == true ]] && [[ -d "$BACKUP_DIR" ]]; then
    echo ""
    print_success "Configuration backup saved to: ${WHITE}$BACKUP_DIR${RESET}"
    echo ""
    echo "${BOLD}${YELLOW}📝 Backup Contents:${RESET}"
    echo "   ${CYAN}•${RESET} Configuration files"
    echo "   ${CYAN}•${RESET} Job database"
    echo "   ${CYAN}•${RESET} Recent log files"
    echo "   ${CYAN}•${RESET} Restoration script (restore.sh)"
    echo ""
    echo "${BOLD}${BLUE}💡 To restore your configuration after reinstalling:${RESET}"
    echo "${WHITE}sudo $BACKUP_DIR/restore.sh${RESET}"
fi

echo ""
echo "${BOLD}${YELLOW}📝 Note: This script does not remove:${RESET}"
echo "   ${CYAN}•${RESET} System packages (cmake, ffmpeg, etc.)"
echo "   ${CYAN}•${RESET} cpr library (if installed from source)"
echo "   ${CYAN}•${RESET} Development tools"
echo ""

echo "${BOLD}${GREEN}✨ DistConv Transcoding Engine has been successfully uninstalled.${RESET}"
echo "${BOLD}${BLUE}Thank you for using DistConv Transcoding Engine!${RESET}"