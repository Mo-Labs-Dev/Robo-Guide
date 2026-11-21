#!/usr/bin/env bash
# Builds the ROS 2 driver workspace, the Robo Guide app and the core unit tests.
# Usage: scripts/build.sh [--clean] [--release]
set -eo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE=RelWithDebInfo
for arg in "$@"; do
  case "$arg" in
    --clean) rm -rf "$ROOT/build" "$ROOT/ros2_ws/build" "$ROOT/ros2_ws/install" "$ROOT/ros2_ws/log" ;;
    --release) BUILD_TYPE=Release ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

# shellcheck disable=SC1091
source /opt/ros/jazzy/setup.bash

echo "== ROS 2 workspace (read-only Waveshare driver) =="
cd "$ROOT/ros2_ws"
colcon build --symlink-install --packages-select robotest_waveshare --cmake-args -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "== Robo Guide application and core library =="
cd "$ROOT"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build build -j"$(nproc)"

echo "== Unit tests =="
ctest --test-dir build --output-on-failure

echo
echo "Build completed: $ROOT/build/desktop/robo_guide/RoboGuide"
