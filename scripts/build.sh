#!/usr/bin/env bash
set -eo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source /opt/ros/jazzy/setup.bash
cd "$ROOT/ros2_ws"
colcon build --symlink-install --packages-select robotest_waveshare
source install/setup.bash
cd "$ROOT/desktop/qt_app"
cmake -S . -B build -G Ninja
cmake --build build -j2
echo "Build completed."
