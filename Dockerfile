# Dock_WMac — vcpkg + mingw cross-compilation environment
# Build: docker build -t dock-wmac-vcpkg .
# Use:   docker run --rm -v $(pwd):/src dock-wmac-vcpkg cmake --preset windows-cross -S /src
#
# Based on Ubuntu 24.04 LTS (Noble Numbat)
FROM ubuntu:24.04

LABEL description="vcpkg cross-compilation environment for Dock_WMac (Qt6 + mingw static)"
LABEL maintainer="Dock_WMac"

# Prevent interactive debconf prompts
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# ============================================================================
# Stage 1: Build-time dependencies (will be cleaned up)
# ============================================================================

# Install all system-level build dependencies in one layer
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Build essentials
    build-essential \
    ca-certificates \
    cmake \
    curl \
    file \
    git \
    make \
    ninja-build \
    pkg-config \
    unzip \
    zip \
    # MinGW-w64 cross-compiler (POSIX threading — required by Qt6)
    g++-mingw-w64-x86-64-posix \
    mingw-w64-x86-64-dev \
    # Build toolchain dependencies (explicit — --no-install-recommends skips them)
    autoconf \
    autoconf-archive \
    automake \
    libtool \
    # Python for vcpkg build scripts
    python3 \
    python3-pip \
    python3-venv \
    # SSL and terminal deps for host build tools
    libncurses-dev \
    libssl-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# ============================================================================
# Stage 2: Install vcpkg
# ============================================================================

ARG VCPKG_TAG=2026.04.27
ARG VCPKG_TRIPLET=x64-mingw-static

# Clone vcpkg at the pinned release tag
RUN git clone --depth 1 --branch ${VCPKG_TAG} https://github.com/microsoft/vcpkg.git /opt/vcpkg

# Bootstrap vcpkg (downloads prebuilt binary)
RUN /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

# Set default triplet globally
ENV VCPKG_DEFAULT_TRIPLET=${VCPKG_TRIPLET}
ENV VCPKG_BINARY_SOURCES="files,/opt/vcpkg-cache,readwrite"

# Pre-install Qt6 for x64-mingw-static (this is the ~2 hour build)
# Features: only gui + widgets (no opengl/dbus/sql/x11 extras)
RUN /opt/vcpkg/vcpkg install \
    --triplet ${VCPKG_TRIPLET} \
    --clean-after-build \
    qtbase[gui,widgets] \
    qtsvg

# ============================================================================
# Stage 3: Cleanup
# ============================================================================

# Remove build intermediate files to reduce image size
RUN find /opt/vcpkg/buildtrees -type f -name '*.o' -delete 2>/dev/null || true \
    && find /opt/vcpkg/buildtrees -type f -name '*.obj' -delete 2>/dev/null || true \
    && find /opt/vcpkg/packages -name '*.cmake' -path '*/debug/*' -delete 2>/dev/null || true

# Remove vcpkg's git history
RUN rm -rf /opt/vcpkg/.git

# Add vcpkg to PATH (convenience)
ENV PATH="/opt/vcpkg:${PATH}"

# Default working directory
WORKDIR /src

# ============================================================================
# Usage examples (all from host, mounting project dir):
#
# Configure + Build:
#   docker run --rm -v $(pwd):/src dock-wmac-vcpkg \
#     sh -c "cmake --preset windows-cross -S /src && cmake --build --preset windows-cross"
#
# Interactive shell:
#   docker run --rm -it -v $(pwd):/src dock-wmac-vcpkg /bin/bash
# ============================================================================
