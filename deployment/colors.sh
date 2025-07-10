#!/bin/bash

# colors.sh - Shared color and formatting functions for WebChess deployment scripts
# Source this file in other scripts with: source "$(dirname "$0")/colors.sh"

# Color codes for terminal output
if [[ -t 1 ]] && command -v tput &> /dev/null; then
    RED=$(tput setaf 1)
    GREEN=$(tput setaf 2)
    YELLOW=$(tput setaf 3)
    BLUE=$(tput setaf 4)
    MAGENTA=$(tput setaf 5)
    CYAN=$(tput setaf 6)
    WHITE=$(tput setaf 7)
    BOLD=$(tput bold)
    RESET=$(tput sgr0)
else
    RED=""
    GREEN=""
    YELLOW=""
    BLUE=""
    MAGENTA=""
    CYAN=""
    WHITE=""
    BOLD=""
    RESET=""
fi

# Print functions with colors
print_header() {
    local title="$1"
    local color="${2:-CYAN}"
    local width=$(tput cols 2>/dev/null || echo 80)
    local min_width=30
    
    # Ensure minimum width
    if [[ $width -lt $min_width ]]; then
        width=$min_width
    fi
    
    local inner_width=$((width - 2))
    local title_length=${#title}
    
    # Handle case where title is longer than available space
    if [[ $title_length -gt $inner_width ]]; then
        title="${title:0:$((inner_width - 3))}..."
        title_length=${#title}
    fi
    
    local padding_total=$((inner_width - title_length))
    local padding_left=$((padding_total / 2))
    local padding_right=$((padding_total - padding_left))
    
    echo ""
    printf "${BOLD}${!color}╔"
    printf "═%.0s" $(seq 1 $inner_width)
    printf "╗${RESET}\n"
    
    printf "${BOLD}${!color}║"
    printf "%*s" $padding_left ""
    printf "%s" "$title"
    printf "%*s" $padding_right ""
    printf "║${RESET}\n"
    
    printf "${BOLD}${!color}╚"
    printf "═%.0s" $(seq 1 $inner_width)
    printf "╝${RESET}\n"
    echo ""
}

print_step() {
    echo "${BOLD}${BLUE}➤ ${1}${RESET}"
}

print_success() {
    echo "${GREEN}✓ ${1}${RESET}"
}

print_warning() {
    echo "${YELLOW}⚠ ${1}${RESET}"
}

print_error() {
    echo "${RED}✗ ${1}${RESET}"
}

print_info() {
    echo "${CYAN}ℹ ${1}${RESET}"
}

# Completion box for successful operations
print_completion_box() {
    local title="$1"
    local width=$(tput cols 2>/dev/null || echo 80)
    local min_width=30
    
    # Ensure minimum width
    if [[ $width -lt $min_width ]]; then
        width=$min_width
    fi
    
    local inner_width=$((width - 2))
    local title_length=${#title}
    
    # Handle case where title is longer than available space
    if [[ $title_length -gt $inner_width ]]; then
        title="${title:0:$((inner_width - 3))}..."
        title_length=${#title}
    fi
    
    local padding_total=$((inner_width - title_length))
    local padding_left=$((padding_total / 2))
    local padding_right=$((padding_total - padding_left))
    
    echo ""
    printf "${BOLD}${GREEN}╔"
    printf "═%.0s" $(seq 1 $inner_width)
    printf "╗${RESET}\n"
    
    printf "${BOLD}${GREEN}║"
    printf "%*s" $padding_left ""
    printf "%s" "$title"
    printf "%*s" $padding_right ""
    printf "║${RESET}\n"
    
    printf "${BOLD}${GREEN}╚"
    printf "═%.0s" $(seq 1 $inner_width)
    printf "╝${RESET}\n"
    echo ""
}