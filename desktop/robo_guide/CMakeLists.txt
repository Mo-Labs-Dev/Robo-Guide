# Robo Guide desktop application (Qt 6 + OpenCV + ROS 2 Jazzy).
# Source /opt/ros/jazzy/setup.bash before configuring so CMake finds rclcpp.

set(CMAKE_AUTOMOC ON)

find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio)

set(RG_SOURCES
    src/main.cpp
    src/AppConfig.cpp src/AppConfig.h
    src/AppContext.cpp src/AppContext.h
    src/ActivityLog.cpp src/ActivityLog.h
    src/RosBridge.cpp src/RosBridge.h
    src/CameraThread.cpp src/CameraThread.h
    src/CameraManager.cpp src/CameraManager.h
    src/PlatformClient.cpp src/PlatformClient.h
    src/JiraXrayClient.cpp src/JiraXrayClient.h
    src/TestLibrary.cpp src/TestLibrary.h
    src/ExecutionRecord.cpp src/ExecutionRecord.h
    src/TestRunner.cpp src/TestRunner.h
    src/UiHelpers.cpp src/UiHelpers.h
    src/MainWindow.cpp src/MainWindow.h
    src/pages/DashboardPage.cpp src/pages/DashboardPage.h
    src/pages/RobotPage.cpp src/pages/RobotPage.h
    src/pages/VisionPage.cpp src/pages/VisionPage.h
    src/pages/TestCasePage.cpp src/pages/TestCasePage.h
    src/pages/TaskDesignerPage.cpp src/pages/TaskDesignerPage.h
    src/pages/AutonomousTestPage.cpp src/pages/AutonomousTestPage.h
    src/pages/PlatformPage.cpp src/pages/PlatformPage.h
    src/pages/JiraXrayPage.cpp src/pages/JiraXrayPage.h
    src/pages/DiagnosticsPage.cpp src/pages/DiagnosticsPage.h
    src/pages/SettingsPage.cpp src/pages/SettingsPage.h
)

add_executable(RoboGuide ${RG_SOURCES})
target_include_directories(RoboGuide PRIVATE src)
# Modern ROS 2 targets (ament_target_dependencies would mix link signatures).
target_link_libraries(RoboGuide PRIVATE
    robo_guide_core
    Qt6::Widgets Qt6::Network
    ${OpenCV_LIBS}
    rclcpp::rclcpp ${std_msgs_TARGETS} ${sensor_msgs_TARGETS})
target_include_directories(RoboGuide SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(RoboGuide PRIVATE -Wall -Wextra)
endif()
