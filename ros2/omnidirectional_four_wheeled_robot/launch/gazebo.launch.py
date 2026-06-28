# ===========================================================================
#  gazebo.launch.py  (ROS 2 / gz)
#
#  Brings up the omnidirectional four-wheeled robot in Gazebo (gz):
#    - gz sim with a world that provides physics / sensors / imu systems
#    - robot_state_publisher (from the xacro-generated robot_description)
#    - spawns the robot (and a sample sphere) into gz
#    - ros2_control spawners: joint_state_broadcaster, position_controller,
#      velocity_controller
#    - controller_kinematics inverse-kinematics node (/cmd_vel -> wheel commands)
#    - ros_gz_bridge for /clock, IMU and camera topics
#    - rviz2 (optional, arg rviz:=true)
# ===========================================================================
import os

import xacro
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    RegisterEventHandler,
)
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg = get_package_share_directory("omnidirectional_four_wheeled_robot")
    ros_gz_sim = get_package_share_directory("ros_gz_sim")

    xacro_file = os.path.join(pkg, "urdf", "robot.urdf.xacro")
    controllers_yaml = os.path.join(pkg, "config", "controllers.yaml")
    world_file = os.path.join(pkg, "worlds", "empty.sdf")
    bridge_config = os.path.join(pkg, "config", "gz_bridge.yaml")
    rviz_config = os.path.join(pkg, "config", "robot.rviz")
    sphere_urdf = os.path.join(pkg, "urdf", "sphere.urdf")

    # Expand the xacro, injecting the absolute path to the controllers config
    # so the gz_ros2_control plugin can find it.
    robot_description_xml = xacro.process_file(
        xacro_file, mappings={"controllers_config": controllers_yaml}
    ).toxml()
    robot_description = {
        "robot_description": robot_description_xml,
        "use_sim_time": True,
    }

    rviz_arg = DeclareLaunchArgument(
        "rviz", default_value="true", description="Launch rviz2"
    )

    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(ros_gz_sim, "launch", "gz_sim.launch.py")
        ),
        launch_arguments={"gz_args": f"-r {world_file}"}.items(),
    )

    robot_state_publisher = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="screen",
        parameters=[robot_description],
    )

    spawn_robot = Node(
        package="ros_gz_sim",
        executable="create",
        output="screen",
        arguments=[
            "-topic", "robot_description",
            "-name", "omnidirectional_four_wheeled_robot",
            "-z", "0.5",
        ],
    )

    spawn_sphere = Node(
        package="ros_gz_sim",
        executable="create",
        output="screen",
        arguments=[
            "-file", sphere_urdf,
            "-name", "sphere",
            "-x", "3", "-y", "3", "-z", "3",
        ],
    )

    bridge = Node(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        output="screen",
        parameters=[{"config_file": bridge_config, "use_sim_time": True}],
    )

    joint_state_broadcaster_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["joint_state_broadcaster"],
        output="screen",
    )

    position_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["position_controller"],
        output="screen",
    )

    velocity_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["velocity_controller"],
        output="screen",
    )

    controller_kinematics = Node(
        package="omnidirectional_four_wheeled_robot",
        executable="controller_kinematics",
        output="screen",
        parameters=[{"use_sim_time": True}],
    )

    rviz = Node(
        package="rviz2",
        executable="rviz2",
        arguments=["-d", rviz_config],
        output="screen",
        condition=IfCondition(LaunchConfiguration("rviz")),
        parameters=[{"use_sim_time": True}],
    )

    # Load controllers in order once the robot is spawned (the controller
    # manager is created by gz_ros2_control when the model appears in gz).
    load_jsb_after_spawn = RegisterEventHandler(
        OnProcessExit(
            target_action=spawn_robot,
            on_exit=[joint_state_broadcaster_spawner],
        )
    )
    load_controllers_after_jsb = RegisterEventHandler(
        OnProcessExit(
            target_action=joint_state_broadcaster_spawner,
            on_exit=[position_controller_spawner, velocity_controller_spawner],
        )
    )

    return LaunchDescription([
        rviz_arg,
        gz_sim,
        robot_state_publisher,
        spawn_robot,
        spawn_sphere,
        bridge,
        load_jsb_after_spawn,
        load_controllers_after_jsb,
        controller_kinematics,
        rviz,
    ])
