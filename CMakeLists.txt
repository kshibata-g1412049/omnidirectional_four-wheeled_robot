cmake_minimum_required(VERSION 3.8)
project(omnidirectional_four_wheeled_robot)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(ros_gz_interfaces REQUIRED)

include_directories(include)

# Inverse-kinematics controller node
add_executable(controller_kinematics src/controller_kinematics.cpp)

# Forward-kinematics odometry publisher node
add_executable(odometry_publisher src/odometry_publisher.cpp)

# Optional sphere spawner (uses ros_gz spawn service at runtime)
add_executable(spawner_sphere src/spawner_sphere.cpp)

# ament_target_dependencies() was removed from ament_cmake on Rolling but is
# still the standard macro on Humble/Jazzy. Detect which linking style is
# available rather than branching on distro name, so this keeps working
# automatically as distros migrate.
if(COMMAND ament_target_dependencies)
  ament_target_dependencies(controller_kinematics
    rclcpp
    geometry_msgs
    std_msgs
  )
  ament_target_dependencies(odometry_publisher
    rclcpp
    sensor_msgs
    nav_msgs
    geometry_msgs
    tf2
    tf2_ros
    tf2_geometry_msgs
  )
  ament_target_dependencies(spawner_sphere
    rclcpp
    ament_index_cpp
    ros_gz_interfaces
  )
else()
  target_link_libraries(controller_kinematics
    rclcpp::rclcpp
    ${geometry_msgs_TARGETS}
    ${std_msgs_TARGETS}
  )
  target_link_libraries(odometry_publisher
    rclcpp::rclcpp
    ${sensor_msgs_TARGETS}
    ${nav_msgs_TARGETS}
    ${geometry_msgs_TARGETS}
    tf2::tf2
    tf2_ros::tf2_ros
    tf2_geometry_msgs::tf2_geometry_msgs
  )
  target_link_libraries(spawner_sphere
    rclcpp::rclcpp
    ament_index_cpp::ament_index_cpp
    ${ros_gz_interfaces_TARGETS}
  )
endif()

install(TARGETS
  controller_kinematics
  odometry_publisher
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
