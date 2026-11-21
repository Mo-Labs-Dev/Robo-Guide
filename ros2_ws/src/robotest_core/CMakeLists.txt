cmake_minimum_required(VERSION 3.8)
project(robotest_core)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
 add_compile_options(-Wall -Wextra -Wpedantic)
endif()
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
add_executable(robot_status_node
 src/robot_status_node.cpp
)
ament_target_dependencies(robot_status_node
 rclcpp
 std_msgs
)
install(TARGETS
 robot_status_node
 DESTINATION lib/${PROJECT_NAME}
)
ament_package()
