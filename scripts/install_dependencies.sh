#!/usr/bin/env bash
# Installs build/runtime dependencies on Ubuntu 24.04 (native or WSL2).
# ROS 2 Jazzy itself must already be installed (see README, "1. Install ROS 2 Jazzy").
set -euo pipefail

if [[ ! -f /opt/ros/jazzy/setup.bash ]]; then
  echo "ERROR: ROS 2 Jazzy not found at /opt/ros/jazzy. Install it first (README step 1)." >&2
  exit 1
fi

sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build git pkg-config \
  qt6-base-dev qt6-base-dev-tools libgl1-mesa-dev \
  libopencv-dev \
  ffmpeg \
  libcurl4-openssl-dev nlohmann-json3-dev \
  python3-colcon-common-extensions

if [[ "${1:-}" == "--with-moveit" ]]; then
  # Phase 2 (robot model + motion planning in simulation). Not needed for Phase 1.
  sudo apt install -y ros-jazzy-moveit ros-jazzy-xacro ros-jazzy-joint-state-publisher-gui ros-jazzy-rviz2
fi

echo "Dependencies installed."
