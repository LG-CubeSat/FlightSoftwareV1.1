#!/usr/bin/env bash
# ==============================================================================
# build_obc.sh - Build Flight Software for Raspberry Pi OBC (HW_MODE)
# ==============================================================================

set -euo pipefail

# Determine repository root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"

echo "=========================================="
echo " Building Flight Software for OBC"
echo " Repo: ${REPO_ROOT}"
echo " Date: $(date)"
echo "=========================================="

# Check and update git submodules if needed
if [ -f ".gitmodules" ]; then
    echo "[BUILD] Syncing and updating git submodules..."
    git submodule update --init --recursive
fi

# Detect build mode (default HW_MODE=ON, pass --sim for SIM_MODE)
HW_MODE="ON"
BUILD_DIR="${REPO_ROOT}/build"

for arg in "$@"; do
    case "$arg" in
        --sim)
            HW_MODE="OFF"
            echo "[BUILD] Simulation mode requested (--sim)."
            ;;
        --clean)
            echo "[BUILD] Cleaning build directory: ${BUILD_DIR}..."
            rm -rf "${BUILD_DIR}"
            ;;
        *)
            ;;
    esac
done

# Detect CPU cores for parallel compilation
CORES=1
if command -v nproc >/dev/null 2>&1; then
    CORES="$(nproc)"
elif command -v sysctl >/dev/null 2>&1; then
    CORES="$(sysctl -n hw.ncpu 2>/dev/null || echo 1)"
fi

echo "[BUILD] Configuring CMake (HW_MODE=${HW_MODE}) with ${CORES} jobs..."
cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DHW_MODE="${HW_MODE}" \
    -DBUILD_TESTS=OFF

echo "[BUILD] Compiling binaries..."
cmake --build "${BUILD_DIR}" -j"${CORES}"

# Verify core OBC binaries exist
BIN_DIR="${BUILD_DIR}/bin"
REQUIRED_BINS=("obc_supervisor" "obc_commands" "obc_compute" "obc_data" "obc_fdir" "obc_mission" "obc_time")

echo "[BUILD] Verifying generated executables in ${BIN_DIR}..."
MISSING=0
for bin in "${REQUIRED_BINS[@]}"; do
    if [ ! -f "${BIN_DIR}/${bin}" ]; then
        echo "[ERROR] Missing binary: ${BIN_DIR}/${bin}"
        MISSING=$((MISSING + 1))
    else
        echo "  - ${bin} [OK]"
    fi
done

if [ "${MISSING}" -ne 0 ]; then
    echo "[BUILD ERROR] ${MISSING} required binaries failed to build!"
    exit 1
fi

echo "=========================================="
echo " Build completed successfully!"
echo " Binaries located in: ${BIN_DIR}"
echo "=========================================="
