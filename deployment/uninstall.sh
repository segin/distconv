#!/bin/bash

# DistConv Dispatch Server Uninstall Script
# Run as root or with sudo privileges

set -e

DISTCONV_USER="distconv"
DISTCONV_HOME="/opt/distconv"
CONFIG_BACKUP_DIR="$HOME/distconv_config_backup_$(date +%Y%m%d_%H%M%S)"

# Source color and formatting functions
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$CURRENT_DIR/colors.sh"

print_header "DistConv Dispatch Server Uninstall" "RED"

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   print_error "This script must be run as root or with sudo"
   exit 1
fi

# Confirmation prompt
echo "${BOLD}${RED}⚠ Warning: This will completely remove DistConv and all its files!${RESET}"
echo ""
echo "${BOLD}${YELLOW}This includes:${RESET}"
echo "   ${CYAN}•${RESET} Application files in ${WHITE}$DISTCONV_HOME${RESET}"
echo "   ${CYAN}•${RESET} Systemd service configuration"
echo "   ${CYAN}•${RESET} Log rotation configuration"
echo "   ${CYAN}•${RESET} All job and engine state data"
echo ""
read -p "${BOLD}Are you sure you want to uninstall DistConv? (y/N): ${RESET}" -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    print_info "Uninstall cancelled."
    exit 0
fi

# Ask about configuration backup
echo ""
read -p "${BOLD}Do you want to backup configuration and state files before uninstalling? (Y/n): ${RESET}" -n 1 -r
echo
if [[ $REPLY =~ ^[Nn]$ ]]; then
    BACKUP_CONFIG=false
else
    BACKUP_CONFIG=true
fi

# Stop and disable service
if systemctl is-active --quiet distconv-dispatch 2>/dev/null; then
    print_step "Stopping DistConv service..."
    systemctl stop distconv-dispatch
fi

if systemctl is-enabled --quiet distconv-dispatch 2>/dev/null; then
    print_step "Disabling DistConv service..."
    systemctl disable distconv-dispatch
fi

# Backup configuration if requested
if [[ $BACKUP_CONFIG == true ]] && [[ -d "$DISTCONV_HOME" ]]; then
    print_step "Backing up configuration to: $CONFIG_BACKUP_DIR"
    mkdir -p "$CONFIG_BACKUP_DIR"
    
    # Backup config files
    if [[ -f "$DISTCONV_HOME/config/server.conf" ]]; then
        cp "$DISTCONV_HOME/config/server.conf" "$CONFIG_BACKUP_DIR/"
        print_success "Backed up server.conf"
    fi
    
    # Backup state file
    if [[ -f "$DISTCONV_HOME/server/state.json" ]]; then
        cp "$DISTCONV_HOME/server/state.json" "$CONFIG_BACKUP_DIR/"
        print_success "Backed up state.json (job and engine data)"
    fi
    
    # Backup entire config directory
    if [[ -d "$DISTCONV_HOME/config" ]]; then
        cp -r "$DISTCONV_HOME/config" "$CONFIG_BACKUP_DIR/"
        print_success "Backed up config directory"
    fi
    
    # Backup logs directory (last 7 days)
    if [[ -d "$DISTCONV_HOME/logs" ]] && [[ "$(ls -A "$DISTCONV_HOME/logs" 2>/dev/null)" ]]; then
        mkdir -p "$CONFIG_BACKUP_DIR/logs"
        find "$DISTCONV_HOME/logs" -name "*.log*" -mtime -7 -exec cp {} "$CONFIG_BACKUP_DIR/logs/" \; 2>/dev/null || true
        print_success "Backed up recent logs"
    fi
    
    # Create restoration script
    cat > "$CONFIG_BACKUP_DIR/restore.sh" << EOF
#!/bin/bash
# DistConv Configuration Restoration Script
# Run this after reinstalling DistConv to restore your settings

set -e

DISTCONV_HOME="/opt/distconv"
BACKUP_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"

if [[ \$EUID -ne 0 ]]; then
   echo "This script must be run as root or with sudo"
   exit 1
fi

echo "Restoring DistConv configuration from backup..."

# Stop service if running
if systemctl is-active --quiet distconv-dispatch 2>/dev/null; then
    systemctl stop distconv-dispatch
fi

# Restore configuration files
if [[ -f "\$BACKUP_DIR/server.conf" ]]; then
    cp "\$BACKUP_DIR/server.conf" "\$DISTCONV_HOME/config/"
    chown distconv:distconv "\$DISTCONV_HOME/config/server.conf"
    echo "✓ Restored server.conf"
fi

if [[ -f "\$BACKUP_DIR/state.json" ]]; then
    cp "\$BACKUP_DIR/state.json" "\$DISTCONV_HOME/server/"
    chown distconv:distconv "\$DISTCONV_HOME/server/state.json"
    echo "✓ Restored state.json"
fi

if [[ -d "\$BACKUP_DIR/config" ]]; then
    cp -r "\$BACKUP_DIR/config"/* "\$DISTCONV_HOME/config/" 2>/dev/null || true
    chown -R distconv:distconv "\$DISTCONV_HOME/config"
    echo "✓ Restored config directory"
fi

# Start service
systemctl start distconv-dispatch
echo "✓ DistConv service restarted"

echo ""
echo "Configuration restoration completed!"
echo "Your DistConv installation should now have all your previous settings."
EOF
    chmod +x "$CONFIG_BACKUP_DIR/restore.sh"
fi

# Remove systemd service file
if [[ -f "/etc/systemd/system/distconv-dispatch.service" ]]; then
    print_step "Removing systemd service file..."
    rm -f /etc/systemd/system/distconv-dispatch.service
    systemctl daemon-reload
fi

# Remove logrotate configuration
if [[ -f "/etc/logrotate.d/distconv" ]]; then
    print_step "Removing log rotation configuration..."
    rm -f /etc/logrotate.d/distconv
fi

# Remove application directory
if [[ -d "$DISTCONV_HOME" ]]; then
    print_step "Removing application directory: $DISTCONV_HOME"
    rm -rf "$DISTCONV_HOME"
fi

# Ask about removing user
echo ""
read -p "${BOLD}Do you want to remove the system user '$DISTCONV_USER'? (y/N): ${RESET}" -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if id "$DISTCONV_USER" &>/dev/null; then
        print_step "Removing system user: $DISTCONV_USER"
        userdel "$DISTCONV_USER"
        print_success "System user removed"
    fi
else
    print_info "System user '$DISTCONV_USER' preserved"
fi

print_completion_box "Uninstall Complete!"

if [[ $BACKUP_CONFIG == true ]] && [[ -d "$CONFIG_BACKUP_DIR" ]]; then
    echo ""
    print_success "Configuration backup saved to: ${WHITE}$CONFIG_BACKUP_DIR${RESET}"
    echo ""
    echo "${BOLD}${YELLOW}📝 Backup Contents:${RESET}"
    echo "   ${CYAN}•${RESET} Configuration files (server.conf)"
    echo "   ${CYAN}•${RESET} State data (jobs and engines)"
    echo "   ${CYAN}•${RESET} Recent log files"
    echo "   ${CYAN}•${RESET} Restoration script (restore.sh)"
    echo ""
    echo "${BOLD}${BLUE}💡 To restore your configuration after reinstalling:${RESET}"
    echo "${WHITE}sudo $CONFIG_BACKUP_DIR/restore.sh${RESET}"
fi

echo ""
echo "${BOLD}${YELLOW}📝 Note: This script does not remove:${RESET}"
echo "   ${CYAN}•${RESET} Build tools (cmake, make, g++)"
echo "   ${CYAN}•${RESET} System packages installed during setup"
echo "   ${CYAN}•${RESET} Nginx configuration (if configured)"
echo "   ${CYAN}•${RESET} Firewall rules"
echo ""

echo "${BOLD}${BLUE}🧹 To completely clean up, you may need to manually remove:${RESET}"
echo "   ${CYAN}•${RESET} Nginx site configuration if you set up a reverse proxy"
echo "   ${CYAN}•${RESET} Firewall rules for port 8080"
echo "   ${CYAN}•${RESET} SSL certificates if you configured HTTPS"
echo ""

echo "${BOLD}${GREEN}✨ DistConv has been successfully uninstalled.${RESET}"
echo "${BOLD}${BLUE}Thank you for using DistConv Dispatch Server!${RESET}"