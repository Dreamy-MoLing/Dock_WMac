#!/usr/bin/env bash
# Dock_WMac — vcpkg cross-compilation helper
# Usage:
#   ./scripts/docker-build.sh              # Build Docker image (first time)
#   ./scripts/docker-build.sh compile      # Cross-compile exe using existing image
#   ./scripts/docker-build.sh shell        # Interactive shell inside container
set -euo pipefail

IMAGE_NAME="dock-wmac-vcpkg"
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

case "${1:-build}" in
    build)
        echo "==> Building Docker image: ${IMAGE_NAME}"
        echo "    This will take ~2 hours (vcpkg builds qtbase + qtsvg from source)"
        docker build -t "${IMAGE_NAME}" -f "${SCRIPT_DIR}/Dockerfile" "${SCRIPT_DIR}"
        echo "==> Done."
        docker images "${IMAGE_NAME}" --format '{{.Size}}'
        ;;
    compile)
        echo "==> Cross-compiling dock_wmac.exe"
        mkdir -p "${SCRIPT_DIR}/build_mxe"
        docker run --rm -v "${SCRIPT_DIR}:/src" "${IMAGE_NAME}" \
            sh -c "cmake --preset windows-cross -S /src && cmake --build --preset windows-cross"
        echo "==> Build complete:"
        ls -lh "${SCRIPT_DIR}/build_mxe/dock_wmac.exe" 2>/dev/null \
            || ls -lh "${SCRIPT_DIR}/build_mxe/src/dock_wmac.exe" 2>/dev/null \
            || echo "    exe not found — check build_mxe/ for output"
        ;;
    shell)
        echo "==> Starting interactive shell in container"
        docker run --rm -it -v "${SCRIPT_DIR}:/src" "${IMAGE_NAME}" /bin/bash
        ;;
    *)
        echo "Usage: $0 [build|compile|shell]"
        exit 1
        ;;
esac
