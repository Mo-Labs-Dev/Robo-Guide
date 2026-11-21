#!/usr/bin/env bash
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source /opt/ros/jazzy/setup.bash
export QT_OPENGL=software LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
exec "$ROOT/desktop/qt_app/build/RoboTestStudio"
