#!/usr/bin/env bash
# Starts the read-only robot driver in the background and the GUI in the foreground.
# The driver is stopped when the GUI exits.
set -eo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$ROOT/logs"
"$ROOT/scripts/start_driver.sh" >"$ROOT/logs/driver.log" 2>&1 &
DRIVER_PID=$!
trap 'kill $DRIVER_PID 2>/dev/null || true' EXIT
echo "Driver started (pid $DRIVER_PID, log: logs/driver.log)"
"$ROOT/scripts/start_gui.sh" "$@"
