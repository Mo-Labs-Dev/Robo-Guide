cmake_minimum_required(VERSION 3.16)
project(RoboGuide VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ROBO_GUIDE_BUILD_APP "Build the Robo Guide desktop application (needs Qt 6, OpenCV, ROS 2)" ON)
option(ROBO_GUIDE_BUILD_TESTS "Build unit tests" ON)
# Physical motion stays OFF until calibration, joint limits and E-stop procedures are validated.
# See docs/SAFETY.md before changing this.
option(ROBO_GUIDE_ENABLE_MOTION "Allow the motion safety gate to open (DO NOT enable before safety sign-off)" OFF)

if(ROBO_GUIDE_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(core)

if(ROBO_GUIDE_BUILD_APP)
    add_subdirectory(desktop/robo_guide)
endif()
