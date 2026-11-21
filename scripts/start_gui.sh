#!/usr/bin/env bash
# Starts Robo Guide. Loads secrets from config/.env (git-ignored) if present.
set -eo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source /opt/ros/jazzy/setup.bash

if [[ -f "$ROOT/config/.env" ]]; then
  set -a
  # shellcheck disable=SC1091
  source "$ROOT/config/.env"
  set +a
fi

export RG_HOME="$ROOT"
# WSLg: software OpenGL avoids blank windows on some GPU drivers (kept from the original script).
if grep -qi microsoft /proc/version 2>/dev/null; then
  export QT_OPENGL=software LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
fi

BIN="$ROOT/build/desktop/robo_guide/RoboGuide"
if [[ ! -x "$BIN" ]]; then
  echo "Robo Guide is not built yet. Run scripts/build.sh first." >&2
  exit 1
fi
exec "$BIN" "$@"
