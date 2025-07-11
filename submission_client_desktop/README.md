# DistConv Submission Client

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/segin/distconv)
[![Qt5](https://img.shields.io/badge/Qt-5.15%2B-green)](https://www.qt.io/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)](https://en.cppreference.com/w/cpp/17)

A user-friendly desktop application for submitting and monitoring video transcoding jobs in the DistConv distributed transcoding system. Built with Qt5/Qt6 for cross-platform compatibility.

## 🖥️ Features

### Core Functionality
- **📁 File Management**: Drag-and-drop interface for video file selection
- **🚀 Job Submission**: Easy submission of transcoding jobs to the dispatch server
- **📊 Real-time Monitoring**: Live status updates and progress tracking
- **📈 Job History**: Complete history of submitted jobs with filtering
- **⚙️ Server Management**: Configure multiple dispatch server endpoints
- **🔧 Preset Management**: Save and reuse common transcoding configurations

### User Experience
- **🎨 Modern UI**: Clean, intuitive interface with responsive design
- **🌓 Theme Support**: Light and dark theme options
- **📱 Cross-platform**: Runs on Linux, Windows, and macOS
- **🔔 Notifications**: System notifications for job completion/failure
- **📋 Batch Operations**: Submit multiple files simultaneously
- **💾 Auto-save**: Automatically saves preferences and session state

## 📋 Table of Contents

- [Screenshots](#-screenshots)
- [Installation](#-installation)
- [Building](#-building)
- [Usage](#-usage)
- [Configuration](#-configuration)
- [Troubleshooting](#-troubleshooting)
- [Development](#-development)

## 📸 Screenshots

### Main Interface
```
┌─────────────────────────────────────────────────────────────┐
│ File  Edit  View  Tools  Help                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  📁 Drop video files here or click to browse               │
│                                                             │
│  ┌─────────────────┐  ┌─────────────────┐                  │
│  │ Source Files    │  │ Target Settings │                  │
│  │ ┌─────────────┐ │  │ Codec: [h264 ▼] │                  │
│  │ │ video1.mp4  │ │  │ Quality: [High] │                  │
│  │ │ video2.avi  │ │  │ Size: [1080p]   │                  │
│  │ │ video3.mov  │ │  │                 │                  │
│  │ └─────────────┘ │  │ [Submit Jobs]   │                  │
│  └─────────────────┘  └─────────────────┘                  │
│                                                             │
│  Job Queue                                                  │
│  ┌─────────────────────────────────────────────────────────┐│
│  │ ID          | File      | Status     | Progress        ││
│  │ job-001     | video1    | Processing | ████████░░ 80%  ││
│  │ job-002     | video2    | Pending    | ░░░░░░░░░░  0%   ││
│  │ job-003     | video3    | Completed  | ██████████ 100% ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

## 🛠️ Installation

### Option 1: Pre-built Binaries

Download the latest release for your platform:

- **Linux**: `distconv-client-linux-x64.AppImage`
- **Windows**: `distconv-client-windows-x64.exe`
- **macOS**: `distconv-client-macos-x64.dmg`

```bash
# Linux
wget https://github.com/segin/distconv/releases/latest/download/distconv-client-linux-x64.AppImage
chmod +x distconv-client-linux-x64.AppImage
./distconv-client-linux-x64.AppImage

# Or install system-wide
sudo cp distconv-client-linux-x64.AppImage /usr/local/bin/distconv-client
```

### Option 2: Package Manager Installation

**Ubuntu/Debian:**
```bash
# Add repository
curl -fsSL https://raw.githubusercontent.com/segin/distconv/main/submission_client_desktop/packaging/gpg.key | sudo apt-key add -
echo "deb https://packages.distconv.io/apt stable main" | sudo tee /etc/apt/sources.list.d/distconv.list

# Install
sudo apt update
sudo apt install distconv-submission-client
```

**Arch Linux:**
```bash
# Install from AUR
yay -S distconv-submission-client
```

**Fedora/CentOS:**
```bash
# Add repository
sudo dnf config-manager --add-repo https://packages.distconv.io/rpm/distconv.repo

# Install
sudo dnf install distconv-submission-client
```

### Option 3: Build from Source

See [Building](#-building) section below.

## 🔨 Building

### Prerequisites

**System Requirements:**
- Qt5 5.15+ or Qt6 6.2+
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.16+
- Git

**Dependencies:**

**Ubuntu/Debian:**
```bash
sudo apt update && sudo apt install -y \
  build-essential cmake pkg-config git \
  qt5-default qtbase5-dev qttools5-dev \
  libqt5network5 libqt5widgets5 \
  libcurl4-openssl-dev nlohmann-json3-dev
```

**Fedora/CentOS:**
```bash
sudo dnf install -y \
  gcc-c++ cmake pkgconfig git \
  qt5-qtbase-devel qt5-qttools-devel \
  libcurl-devel json-devel
```

**Windows (MSYS2):**
```bash
pacman -S mingw-w64-x86_64-qt5 \
          mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-curl \
          mingw-w64-x86_64-nlohmann-json
```

**macOS:**
```bash
brew install qt5 cmake curl nlohmann-json
export PATH="/usr/local/opt/qt5/bin:$PATH"
```

### Build Steps

**Basic Build:**
```bash
# Clone repository
git clone https://github.com/segin/distconv.git
cd distconv/submission_client_desktop

# Create build directory
mkdir build && cd build

# Configure
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
make -j$(nproc)

# Run
./submission_client_desktop
```

**Qt6 Build:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_QT6=ON ..
make -j$(nproc)
```

**Windows Build:**
```bash
# Using Visual Studio
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release

# Using MinGW
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
mingw32-make -j4
```

**Static Build (for distribution):**
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DSTATIC_BUILD=ON ..
make -j$(nproc)
```

### Testing

```bash
# Run unit tests
make test

# Or run directly
./submission_client_desktop_tests

# Run specific tests
./submission_client_desktop_tests --gtest_filter="*NetworkTest*"
```

### Packaging

**Linux AppImage:**
```bash
# Build static version
cmake -DCMAKE_BUILD_TYPE=Release -DSTATIC_BUILD=ON ..
make -j$(nproc)

# Create AppImage
make appimage
```

**Windows Installer:**
```bash
# Install NSIS
# Build with NSIS support
cmake -DCMAKE_BUILD_TYPE=Release -DPACKAGE_WINDOWS=ON ..
make -j$(nproc)
cpack
```

**macOS DMG:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DPACKAGE_MACOS=ON ..
make -j$(nproc)
cpack
```

## 📱 Usage

### First Time Setup

1. **Launch the application**
2. **Configure server connection**:
   - Go to `Settings` → `Server Configuration`
   - Enter dispatch server URL (e.g., `http://dispatch.example.com:8080`)
   - Enter API key
   - Test connection

3. **Set preferences**:
   - Default output directory
   - Notification preferences
   - Theme selection
   - Auto-refresh intervals

### Basic Workflow

#### 1. Add Video Files

**Drag and Drop:**
- Drag video files from file manager into the main window
- Files are automatically validated and added to the queue

**File Browser:**
- Click "Add Files" or `Ctrl+O`
- Select one or more video files
- Supported formats: MP4, AVI, MOV, MKV, WMV, FLV

#### 2. Configure Transcoding Settings

**Quick Presets:**
- Select from predefined presets (Web Optimized, High Quality, Mobile, etc.)
- Customize codec, resolution, and quality settings

**Advanced Settings:**
- Codec selection (H.264, H.265, VP9, AV1)
- Resolution and aspect ratio
- Bitrate and quality settings
- Audio codec and settings

#### 3. Submit Jobs

- Click "Submit Jobs" or `Ctrl+Enter`
- Jobs are submitted to the dispatch server
- Progress tracking begins automatically

#### 4. Monitor Progress

**Real-time Updates:**
- Job status updates every 30 seconds (configurable)
- Progress bars show transcoding progress
- Notifications for completion/failure

**Job Management:**
- Cancel pending jobs
- Retry failed jobs
- Download completed files
- View detailed job information

### Advanced Features

#### Batch Processing

```bash
# Command-line batch mode
./submission_client_desktop --batch \
  --server "http://dispatch.example.com:8080" \
  --api-key "your-key" \
  --codec "h264" \
  --preset "web" \
  /path/to/videos/*.mp4
```

#### Automation

**Watch Folder:**
- Configure automatic processing of files in specified directories
- Set up rules for automatic codec and quality selection
- Schedule processing during off-peak hours

**Integration:**
- REST API for programmatic job submission
- Command-line interface for scripting
- Plugin system for custom workflows

## ⚙️ Configuration

### Settings File Location

**Linux:** `~/.config/DistConv/SubmissionClient.conf`
**Windows:** `%APPDATA%/DistConv/SubmissionClient.ini`
**macOS:** `~/Library/Preferences/com.distconv.SubmissionClient.plist`

### Configuration Options

**Server Settings:**
```ini
[Server]
url=http://dispatch.example.com:8080
api_key=your-secure-api-key
timeout=30
retry_attempts=3
```

**Application Settings:**
```ini
[Application]
theme=dark
auto_refresh=true
refresh_interval=30
notifications=true
output_directory=/home/user/transcoded
max_concurrent_uploads=3
```

**Transcoding Defaults:**
```ini
[Defaults]
codec=h264
quality=high
resolution=1080p
audio_codec=aac
preset=web_optimized
```

### Environment Variables

```bash
export DISTCONV_SERVER_URL="http://dispatch.example.com:8080"
export DISTCONV_API_KEY="your-secure-api-key"
export DISTCONV_OUTPUT_DIR="/path/to/output"
export DISTCONV_THEME="dark"
```

## 🔧 Troubleshooting

### Common Issues

#### Application Won't Start

```bash
# Check dependencies
ldd submission_client_desktop

# Check Qt installation
qmake --version

# Run with debug output
./submission_client_desktop --debug

# Check configuration
cat ~/.config/DistConv/SubmissionClient.conf
```

#### Connection Issues

**Cannot connect to server:**
```bash
# Test connectivity
curl -H "X-API-Key: your-key" http://dispatch.example.com:8080/health

# Check DNS resolution
nslookup dispatch.example.com

# Test with different network
# Check firewall settings
```

**SSL/TLS errors:**
```bash
# Update certificates
sudo apt update && sudo apt install ca-certificates

# Check SSL configuration
openssl s_client -connect dispatch.example.com:443
```

#### File Upload Issues

**Large file timeouts:**
- Increase timeout in settings
- Check network bandwidth
- Use wired connection for large files

**Permission errors:**
```bash
# Check file permissions
ls -la /path/to/video/files

# Check output directory permissions
mkdir -p ~/transcoded && chmod 755 ~/transcoded
```

#### Performance Issues

**High memory usage:**
```bash
# Monitor memory
top -p $(pgrep submission_client)

# Reduce concurrent uploads
# Settings → Application → Max Concurrent Uploads: 1
```

**Slow UI response:**
```bash
# Disable auto-refresh temporarily
# Settings → Application → Auto Refresh: false

# Increase refresh interval
# Settings → Application → Refresh Interval: 60
```

### Debug Mode

```bash
# Enable debug logging
./submission_client_desktop --log-level=debug

# Save logs to file
./submission_client_desktop --log-file=/tmp/distconv-client.log

# Verbose network logging
./submission_client_desktop --network-debug
```

### Log Analysis

**Log file locations:**
- **Linux:** `~/.local/share/DistConv/logs/`
- **Windows:** `%LOCALAPPDATA%/DistConv/logs/`
- **macOS:** `~/Library/Logs/DistConv/`

```bash
# View recent logs
tail -f ~/.local/share/DistConv/logs/submission_client.log

# Search for errors
grep -i error ~/.local/share/DistConv/logs/submission_client.log

# Network debugging
grep "HTTP" ~/.local/share/DistConv/logs/submission_client.log
```

## 🔬 Development

### Development Environment

```bash
# Clone repository
git clone https://github.com/segin/distconv.git
cd distconv/submission_client_desktop

# Install Qt Creator (optional)
sudo apt install qtcreator

# Open project
qtcreator CMakeLists.txt
```

### Code Structure

```
submission_client_desktop/
├── src/
│   ├── main.cpp                 # Application entry point
│   ├── mainwindow.{cpp,h}       # Main application window
│   ├── jobmanager.{cpp,h}       # Job submission and tracking
│   ├── servermanager.{cpp,h}    # Server communication
│   ├── settings.{cpp,h}         # Configuration management
│   └── widgets/                 # Custom UI widgets
├── ui/
│   ├── mainwindow.ui           # Main window layout
│   ├── settings.ui             # Settings dialog
│   └── jobdetails.ui           # Job details dialog
├── resources/
│   ├── icons/                  # Application icons
│   ├── themes/                 # UI themes
│   └── translations/           # Internationalization
├── tests/
│   ├── test_jobmanager.cpp     # Unit tests
│   └── test_servermanager.cpp
└── packaging/
    ├── linux/                  # Linux packaging scripts
    ├── windows/                # Windows packaging scripts
    └── macos/                  # macOS packaging scripts
```

### Build System

**CMake configuration supports:**
- Static and dynamic linking
- Qt5 and Qt6 compatibility
- Cross-platform compilation
- Automated testing
- Package generation

### Testing

```bash
# Unit tests
ctest

# Integration tests
./tests/integration_tests

# UI tests (requires X11/Wayland)
./tests/ui_tests

# Memory leak testing
valgrind --tool=memcheck ./submission_client_desktop
```

### Internationalization

```bash
# Extract translatable strings
lupdate src/ -ts resources/translations/distconv_en.ts

# Update translations
linguist resources/translations/distconv_en.ts

# Generate binary translations
lrelease resources/translations/*.ts
```

### Custom Builds

**Minimal build (no GUI):**
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DHEADLESS_MODE=ON ..
```

**Plugin-enabled build:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_PLUGINS=ON ..
```

## 📊 Features Roadmap

### Planned Features
- **🔄 Auto-retry with backoff**: Intelligent retry logic for failed uploads
- **📊 Analytics dashboard**: Job statistics and performance metrics
- **🔌 Plugin system**: Extensible architecture for custom workflows
- **🌐 Web interface**: Browser-based remote access
- **📱 Mobile companion**: iOS/Android monitoring app
- **🤖 AI optimization**: Automatic quality and codec selection
- **☁️ Cloud integration**: Direct upload to cloud storage

## 🤝 Contributing

### Getting Started

```bash
# Fork the repository
# Clone your fork
git clone https://github.com/yourusername/distconv.git
cd distconv/submission_client_desktop

# Create feature branch
git checkout -b feature/your-feature-name

# Make changes and commit
git commit -m "Add your feature"

# Push and create pull request
git push origin feature/your-feature-name
```

### Development Guidelines

- Follow Qt coding conventions
- Add unit tests for new features
- Update documentation for API changes
- Test on multiple platforms before submitting
- Use Qt's signal/slot mechanism for async operations

### Bug Reports

When reporting bugs, include:
- Operating system and version
- Qt version and compiler
- Application version
- Steps to reproduce
- Log files (with sensitive data removed)

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](../LICENSE) file for details.

## 🆘 Support

- **Issues**: [GitHub Issues](https://github.com/segin/distconv/issues)
- **Documentation**: [Main Project README](../README.md)
- **User Guide**: [User Guide](docs/user-guide.md)

---

**DistConv Submission Client** - Professional video transcoding made simple