#!/usr/bin/env bash
# ==============================================================================
# install_service.sh - Install and start obc.service with current user and paths
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CURRENT_USER="$(id -un)"

echo "Installing OBC systemd service for user '${CURRENT_USER}' at '${REPO_ROOT}'..."

SERVICE_FILE="/tmp/obc.service"

cat <<EOF > "${SERVICE_FILE}"
[Unit]
Description=Flight Software OBC Supervisor Service
After=network.target local-fs.target pigpiod.service
Wants=pigpiod.service

[Service]
Type=simple
User=${CURRENT_USER}
WorkingDirectory=${REPO_ROOT}
ExecStart=${REPO_ROOT}/scripts/run_obc.sh
Restart=always
RestartSec=3
KillMode=mixed
TimeoutStopSec=10

StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

sudo cp "${SERVICE_FILE}" /etc/systemd/system/obc.service
sudo systemctl daemon-reload
sudo systemctl enable obc.service
sudo systemctl restart obc.service

echo "OBC service installed and started successfully!"
echo "Check status with: sudo systemctl status obc.service"
