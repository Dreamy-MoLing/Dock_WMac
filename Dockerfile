# ============================================================================
# Dock_WMac cross-compilation Docker image
# Based on openscad/mxe-x86_64-gui (MXE + Qt6 pre-compiled)
#
# Build:  docker build -t dock-wmac-mxe .
# Use:    docker run --rm -v $(pwd):/src dock-wmac-mxe \
#           sh -c "cmake -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchain-mxe-win64.cmake \
#                        -DCMAKE_BUILD_TYPE=Release -B /src/build_win -S /src && \
#                   cmake --build /src/build_win"
# ============================================================================

FROM openscad/mxe-x86_64-gui:latest

LABEL description="Cross-compilation environment for Dock_WMac (Qt6 + MXE + Windows x64)"
LABEL maintainer="Dock_WMac"

# Prevent interactive debconf prompts
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install CMake and Ninja (build system inside the container)
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    ninja-build \
    file \
    pkg-config \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Default working directory
WORKDIR /src

# ============================================================================
# Usage (from host):
#
#   docker run --rm -v $(pwd):/src dock-wmac-mxe \
#     sh -c "cmake -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchain-mxe-win64.cmake \
#                  -DCMAKE_BUILD_TYPE=Release -B /src/build_win -S /src && \
#             cmake --build /src/build_win"
#
# To get a shell inside the container:
#   docker run --rm -it -v $(pwd):/src dock-wmac-mxe /bin/bash
# ============================================================================
