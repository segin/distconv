#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

echo "Starting Distributed Video Transcoding System Quickstart..."

# --- 1. Install Prerequisites ---
echo "
--- Installing Prerequisites ---"
sudo apt-get update
sudo apt-get install -y git cmake build-essential curl ffmpeg libcjson-dev libwxgtk3.2-dev libwxbase3.2-dev openssl libcurl4-openssl-dev libsqlite3-dev libgtest-dev

# --- 2. Generate SSL Certificates ---
echo "
--- Generating Self-Signed SSL Certificates ---"
# Check if certificates already exist to avoid overwriting
if [ ! -f server.key ] || [ ! -f server.csr ] || [ ! -f server.crt ]; then
    openssl genrsa -out server.key 2048
    openssl req -new -key server.key -out server.csr -subj "/CN=localhost"
    openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt
    echo "SSL certificates generated: server.key, server.csr, server.crt"
else
    echo "SSL certificates already exist. Skipping generation."
fi

# --- 3. Build C++ Components ---
echo "
--- Building C++ Components ---"

# Build Transcoding Engine
echo "Building Transcoding Engine..."
mkdir -p transcoding_engine/build
cd transcoding_engine/build
cmake ..
make
cd ../..
echo "Transcoding Engine built successfully."

# Build C++ Dispatch Server
echo "Building C++ Dispatch Server..."
mkdir -p dispatch_server_cpp/build
cd dispatch_server_cpp/build
cmake ..
make
cd ../..
echo "C++ Dispatch Server built successfully."

# Build C++ Submission Client
echo "Building C++ Submission Client..."
mkdir -p submission_client_desktop/build
cd submission_client_desktop/build
cmake ..
make
cd ../..
echo "C++ Submission Client built successfully."

echo "
--- Quickstart Complete ---"
echo "You can now run the components:"
echo "1. Start the C++ Dispatch Server: cd dispatch_server_cpp/build && ./dispatch_server_cpp"
echo "2. Start the Transcoding Engine: cd transcoding_engine/build && ./transcoding_engine --ca-cert ../../server.crt --dispatch-url https://localhost:8080"
echo "3. Run the C++ Submission Client: cd submission_client_desktop/build && ./submission_client_desktop --ca-cert ../../server.crt"

