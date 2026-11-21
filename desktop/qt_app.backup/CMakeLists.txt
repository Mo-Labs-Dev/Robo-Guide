cmake_minimum_required(VERSION 3.16)
project(RoboTestStudio VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(ament_cmake REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
add_executable(RoboTestStudio
   main.cpp
   mainwindow.cpp
   mainwindow.h
)
ament_target_dependencies(RoboTestStudio
   rclcpp
   std_msgs
   sensor_msgs
)
target_link_libraries(RoboTestStudio
   Qt6::Widgets
   Qt6::Network
)
