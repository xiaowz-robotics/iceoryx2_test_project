cmake_minimum_required(VERSION 3.8)
project(ros2_image_stress_test)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

add_executable(ros_image_publisher src/ros_image_publisher.cpp)
ament_target_dependencies(ros_image_publisher
  rclcpp
  sensor_msgs
)

add_executable(ros_image_subscriber src/ros_image_subscriber.cpp)
ament_target_dependencies(ros_image_subscriber
  rclcpp
  sensor_msgs
)

install(TARGETS
  ros_image_publisher
  ros_image_subscriber
  DESTINATION lib/${PROJECT_NAME}
)

install(PROGRAMS
  scripts/run_ros2_stress.sh
  DESTINATION share/${PROJECT_NAME}/scripts
)

ament_package()