# Multi-stage Dockerfile for DistConv Transcoding Engine
# Modern C++ implementation with dependency injection

# Build stage
FROM ubuntu:22.04 AS builder

# Avoid interactive prompts during build
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    curl \
    wget \
    libsqlite3-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    libgmock-dev \
    && rm -rf /var/lib/apt/lists/*

# Install cpr library from source
WORKDIR /tmp
RUN git clone https://github.com/libcpr/cpr.git && \
    cd cpr && \
    mkdir build && cd build && \
    cmake .. -DCPR_USE_SYSTEM_CURL=ON -DBUILD_SHARED_LIBS=ON && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Copy source code
WORKDIR /app
COPY src/ src/
COPY CMakeLists_modern.txt CMakeLists.txt
COPY app_main_modern.cpp .

# Build the application
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make test

# Runtime stage
FROM ubuntu:22.04 AS runtime

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    ffmpeg \
    libsqlite3-0 \
    libssl3 \
    libcurl4 \
    nlohmann-json3-dev \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install cpr runtime library
COPY --from=builder /usr/local/lib/libcpr* /usr/local/lib/
RUN ldconfig

# Create non-root user
RUN groupadd -r distconv && useradd -r -g distconv -s /bin/false distconv

# Create application directories
RUN mkdir -p /app/{bin,config,logs,data,temp} && \
    chown -R distconv:distconv /app

# Copy built application
COPY --from=builder /app/build/transcoding_engine_modern /app/bin/
COPY --from=builder /app/build/libTranscodingEngine* /usr/local/lib/ 2>/dev/null || true

# Copy configuration template
COPY deployment/docker-config.json /app/config/engine.json

# Set working directory
WORKDIR /app

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD pgrep -f transcoding_engine_modern || exit 1

# Environment variables
ENV DISTCONV_ENGINE_HOME=/app
ENV DISTCONV_CONFIG_FILE=/app/config/engine.json
ENV DISTCONV_LOG_DIR=/app/logs
ENV DISTCONV_DATA_DIR=/app/data
ENV DISTCONV_TEMP_DIR=/app/temp

# Expose any ports if needed (none for transcoding engine)
# EXPOSE 8080

# Switch to non-root user
USER distconv

# Entry point
ENTRYPOINT ["/app/bin/transcoding_engine_modern"]

# Default command arguments
CMD ["--config", "/app/config/engine.json"]

# Metadata
LABEL maintainer="DistConv Team"
LABEL version="1.0.0"
LABEL description="DistConv Transcoding Engine - Modern C++ Implementation"
LABEL org.opencontainers.image.title="DistConv Transcoding Engine"
LABEL org.opencontainers.image.description="Modern C++ transcoding engine with dependency injection"
LABEL org.opencontainers.image.version="1.0.0"
LABEL org.opencontainers.image.vendor="DistConv"
LABEL org.opencontainers.image.licenses="MIT"
LABEL org.opencontainers.image.source="https://github.com/segin/distconv"

# Build information
ARG BUILD_DATE
ARG VCS_REF
LABEL org.opencontainers.image.created=$BUILD_DATE
LABEL org.opencontainers.image.revision=$VCS_REF