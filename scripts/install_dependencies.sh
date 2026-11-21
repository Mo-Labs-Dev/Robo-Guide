#!/usr/bin/env bash
set -e
sudo apt update
sudo apt install -y build-essential cmake ninja-build qt6-base-dev libcurl4-openssl-dev nlohmann-json3-dev python3-colcon-common-extensions
