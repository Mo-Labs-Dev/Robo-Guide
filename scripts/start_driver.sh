#!/usr/bin/env bash
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source /opt/ros/jazzy/setup.bash
source "$ROOT/ros2_ws/install/setup.bash"
exec ros2 run robotest_waveshare waveshare_driver_node
