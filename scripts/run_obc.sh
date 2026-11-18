#!/usr/bin/env bash
# ==============================================================================
# run_obc.sh - Startup Execution Script for Flight Software OBC
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"

BIN_DIR="${REPO_ROOT}/build/bin"
LOG_DIR="${REPO_ROOT}/logs"
SUPERVISOR="${BIN_DIR}/obc_supervisor"

echo "=========================================="
echo " Starting Flight Software OBC Supervisor"
echo " Time: $(date)"
echo " Root: ${REPO_ROOT}"
echo "=========================================="

# 1. Compile binaries if not present
if [ ! -f "${SUPERVISOR}" ]; then
    echo "[STARTUP] ${SUPERVISOR} not found. Running build_obc.sh first..."
    "${SCRIPT_DIR}/build_obc.sh"
fi

# 2. On Linux, ensure pigpiod daemon is running for hardware GPIO/UART access
if [ "$(uname -s)" = "Linux" ]; then
    if ! pgrep -x "pigpiod" >/dev/null 2>&1; then
        echo "[STARTUP] pigpiod is not running. Attempting to start daemon..."
        if command -v systemctl >/dev/null 2>&1 && systemctl is-enabled pigpiod >/dev/null 2>&1; then
            sudo systemctl start pigpiod || true
        else
            sudo pigpiod || true
        fi
        sleep 0.5
    fi

    if pgrep -x "pigpiod" >/dev/null 2>&1; then
        echo "[STARTUP] pigpiod daemon is active."
    else
        echo "[WARNING] pigpiod could not be started automatically. E22 radio UART may fail if needed."
    fi
fi

# 3. Clean up stale IPC sockets in /tmp from previous runs
echo "[STARTUP] Cleaning up stale IPC domain sockets in /tmp..."
rm -f /tmp/obc_*.sock /tmp/comms_i2c.sock /tmp/supervisor_*.sock 2>/dev/null || true

# 4. Prepare logs directory
mkdir -p "${LOG_DIR}"
TIMESTAMP="$(date +'%Y%m%d_%H%M%S')"
LOG_FILE="${LOG_DIR}/obc_${TIMESTAMP}.log"

echo "[STARTUP] Launching supervisor: ${SUPERVISOR}"
echo "[STARTUP] Logging to: ${LOG_FILE}"
echo "=========================================="

# 5. Launch OBC Supervisor
# If running under an interactive shell, tee to both terminal and log file.
# If running under systemd or non-interactive, execute directly so supervisor owns PID 1/process tree.
if [ -t 1 ]; then
    exec "${SUPERVISOR}" 2>&1 | tee -a "${LOG_FILE}"
else
    exec "${SUPERVISOR}"
fi
