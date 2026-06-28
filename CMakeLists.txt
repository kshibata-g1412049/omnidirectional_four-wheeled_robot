cmake_minimum_required(VERSION 3.8)
project(omnidirectional_four_wheeled_robot)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(ros_gz_interfaces REQUIRED)

# Inverse-kinematics controller node
add_executable(controller_kinematics src/controller_kinematics.cpp)
ament_target_dependencies(controller_kinematics
  rclcpp
  geometry_msgs
  std_msgs
)

# Optional sphere spawner (uses ros_gz spawn service at runtime)
add_executable(spawner_sphere src/spawner_sphere.cpp)
ament_target_dependencies(spawner_sphere
  rclcpp
  ament_index_cpp
  ros_gz_interfaces
)

install(TARGETS
  controller_kinematics
  spawner_sphere
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch urdf config worlds
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
