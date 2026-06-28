# ===========================================================================
#  display.launch.py  (ROS 2)
#
#  Visualises the robot model in rviz2 without Gazebo:
#    - robot_state_publisher (from the xacro-generated robot_description)
#    - joint_state_publisher_gui (sliders to move the wheel joints)
#    - rviz2
# ===========================================================================
import os

import xacro
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    pkg = get_package_share_directory("omnidirectional_four_wheeled_robot")
    xacro_file = os.path.join(pkg, "urdf", "robot.urdf.xacro")
    controllers_yaml = os.path.join(pkg, "config", "controllers.yaml")
    rviz_config = os.path.join(pkg, "config", "robot.rviz")

    robot_description_xml = xacro.process_file(
        xacro_file, mappings={"controllers_config": controllers_yaml}
    ).toxml()

    robot_state_publisher = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="screen",
        parameters=[{"robot_description": robot_description_xml}],
    )

    joint_state_publisher_gui = Node(
        package="joint_state_publisher_gui",
        executable="joint_state_publisher_gui",
        output="screen",
    )

    rviz = Node(
        package="rviz2",
        executable="rviz2",
        arguments=["-d", rviz_config],
        output="screen",
    )

    return LaunchDescription([
        robot_state_publisher,
        joint_state_publisher_gui,
        rviz,
    ])
