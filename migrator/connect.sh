#!/bin/bash
#
# connect.sh — run the tfp0 probe on a USB-connected checkra1n device.
#
# Run this ON THE COMPUTER (macOS/Linux) with the jailbroken iPad plugged in via
# USB and checkra1n fully applied (jailbroken, not just booted).
#
# Requirements on the computer:
#   - iproxy  (macOS: `brew install libusbmuxd`  |  Linux: `apt install libusbmuxd-tools`)
#   - ssh / scp
#   - the `tfp0probe` binary in the same folder as this script
#     (download it from the GitHub Actions "tfp0probe" artifact)
#
# checkra1n's SSH (Dropbear) listens on device port 44 over USB; user root,
# password "alpine". If yours is on 22, pass it as the first argument.
#
set -euo pipefail

DEVICE_SSH_PORT="${1:-44}"
LOCAL_PORT=2222
HERE="$(cd "$(dirname "$0")" && pwd)"
PROBE="$HERE/tfp0probe"

if [ ! -f "$PROBE" ]; then
  echo "ERROR: $PROBE not found. Download the 'tfp0probe' artifact and put it next to this script." >&2
  exit 1
fi

if ! command -v iproxy >/dev/null 2>&1; then
  echo "ERROR: iproxy not found. Install it (macOS: brew install libusbmuxd)." >&2
  exit 1
fi

echo "[*] Tunneling USB SSH: device :$DEVICE_SSH_PORT  ->  localhost:$LOCAL_PORT"
iproxy "$LOCAL_PORT" "$DEVICE_SSH_PORT" >/dev/null 2>&1 &
IPROXY_PID=$!
trap 'kill "$IPROXY_PID" 2>/dev/null || true' EXIT
sleep 2

SSH_OPTS="-p $LOCAL_PORT -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"

echo "[*] Copying probe to device (password is usually: alpine)"
scp $SSH_OPTS "$PROBE" root@localhost:/tmp/tfp0probe

echo "[*] Running probe as root on device:"
echo "------------------------------------------------------------"
ssh $SSH_OPTS root@localhost "chmod +x /tmp/tfp0probe && /tmp/tfp0probe; echo; echo exit_code=\$?; rm -f /tmp/tfp0probe"
echo "------------------------------------------------------------"
echo "[*] Done. Copy the whole output above back to the chat."
