#!/usr/bin/env bash
# =============================================================================
# Dock_WMac — MXE cross-compilation helper
# Usage:
#   ./scripts/docker-build.sh build    # Build the Docker image (once)
#   ./scripts/docker-build.sh compile  # Cross-compile dock_wmac.exe
#   ./scripts/docker-build.sh clean    # Remove build artifacts
#   ./scripts/docker-build.sh shell    # Enter container interactively
# =============================================================================
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE_NAME="dock-wmac-mxe"
BUILD_DIR="build_win"
TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/toolchain-mxe-win64.cmake"

# ---------------------------------------------------------------------------
# Host system — check prerequisites
# ---------------------------------------------------------------------------
check_prereqs() {
    local missing=0
    for cmd in docker cmake; do
        if ! command -v "$cmd" &>/dev/null; then
            echo "[ERROR] '$cmd' not found. Install it first."
            missing=1
        fi
    done
    if [ "$missing" -ne 0 ]; then
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# Build the Docker image
# ---------------------------------------------------------------------------
cmd_build() {
    echo "[BUILD] Building Docker image '${IMAGE_NAME}'..."
    docker build -t "${IMAGE_NAME}" "${PROJECT_ROOT}"
    echo "[BUILD] Done."
}

# ---------------------------------------------------------------------------
# Compile dock_wmac.exe inside the container
# ---------------------------------------------------------------------------
cmd_compile() {
    if ! docker image inspect "${IMAGE_NAME}" &>/dev/null; then
        echo "[INFO] Image '${IMAGE_NAME}' not found. Building first..."
        cmd_build
    fi

    echo "[COMPILE] Cross-compiling dock_wmac.exe..."
    docker run --rm -v "${PROJECT_ROOT}:/src" "${IMAGE_NAME}" \
        sh -c "
            cmake -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchain-mxe-win64.cmake \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DBUILD_TESTS=OFF \
                  -B /src/${BUILD_DIR} \
                  -S /src \
                  -G 'Unix Makefiles' \
            && cmake --build /src/${BUILD_DIR} -j\$(nproc)
        "

    local exe="${PROJECT_ROOT}/${BUILD_DIR}/dock_wmac.exe"
    if [ -f "${exe}" ]; then
        echo "[COMPILE] Success! Binary: ${exe}"
        file "${exe}"
    else
        echo "[ERROR] Build failed — ${exe} not found."
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# Clean build artifacts
# ---------------------------------------------------------------------------
cmd_clean() {
    echo "[CLEAN] Removing build directory '${BUILD_DIR}'..."
    rm -rf "${PROJECT_ROOT:?}/${BUILD_DIR}"
    echo "[CLEAN] Done."
}

# ---------------------------------------------------------------------------
# Interactive shell inside the container
# ---------------------------------------------------------------------------
cmd_shell() {
    if ! docker image inspect "${IMAGE_NAME}" &>/dev/null; then
        echo "[INFO] Image '${IMAGE_NAME}' not found. Building first..."
        cmd_build
    fi

    echo "[SHELL] Entering container..."
    docker run --rm -it -v "${PROJECT_ROOT}:/src" "${IMAGE_NAME}" /bin/bash
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
check_prereqs

case "${1:-help}" in
    build)
        cmd_build
        ;;
    compile)
        cmd_compile
        ;;
    clean)
        cmd_clean
        ;;
    shell)
        cmd_shell
        ;;
    *)
        echo "Usage: $0 {build|compile|clean|shell}"
        exit 1
        ;;
esac
