# omnidirectional_four_wheeled_robot (ROS 2)

ROS 2 (**Humble**, **Jazzy**, or **Rolling** -- auto-detected via `$ROS_DISTRO`,
no per-distro file forks) + **Gazebo (gz)** package for an omnidirectional
four-wheeled (swerve-drive) robot. Each wheel has an independent steering joint
(`*_joint1`, position controlled) and a drive joint (`*_joint2`, velocity
controlled). The `controller_kinematics` node converts a body twist on
`/cmd_vel` into per-wheel steering angles and rotation speeds. The
`odometry_publisher` node does the inverse: it reads `/joint_states` and
publishes `/odom` + the `odom` -> `base_link` TF (forward kinematics).

> This repository was originally a ROS 1 (catkin) package. It has been ported to
> ROS 2; the legacy ROS 1 files have been removed and this package now lives at
> the repository root.

## Layout

```
package.xml  CMakeLists.txt          # ament_cmake package
src/                                 # controller_kinematics, odometry_publisher, spawner_sphere (rclcpp)
include/                             # shared wheel-geometry constants (robot_geometry.hpp)
urdf/                                # robot/wheel/camera/laser xacro + sphere.urdf
config/                              # controllers.yaml, gz_bridge.yaml, robot.rviz
worlds/empty.sdf                     # gz world with physics/sensors/imu systems
launch/                             # gazebo.launch.py, display.launch.py
Dockerfile  scripts/                 # reproducible build + headless smoke test
```

## Quick start with Docker (recommended)

Builds the workspace and runs a headless functional check (no GUI required):

```bash
bash scripts/docker_build_and_test.sh
```

This builds the image (`omni4wd:jazzy`) and runs `scripts/smoke_test.sh` inside
the container, which verifies that the three controllers reach `active` and that
publishing `/cmd_vel` makes the wheel drive joints rotate in gz.

### Choosing a ROS 2 distro

Humble, Jazzy, and Rolling are all supported; the distro is detected
automatically at build time from `$ROS_DISTRO` inside the container (set by
the official `ros:<distro>-ros-base` base image) -- the Dockerfile and scripts
are shared across all three, no per-distro file forks.

```bash
# Jazzy (default)
bash scripts/docker_build_and_test.sh

# Humble
bash scripts/docker_build_and_test.sh omni4wd:humble humble

# Rolling
bash scripts/docker_build_and_test.sh omni4wd:rolling rolling
```

### Windows host

No WSL or Git Bash required -- use the PowerShell equivalent from a regular
PowerShell prompt (with Docker Desktop running):

```powershell
.\scripts\docker_build_and_test.ps1
.\scripts\docker_build_and_test.ps1 -ImageTag omni4wd:humble -RosDistro humble
```

Interactive use of the image:

```bash
docker run --rm -it omni4wd:jazzy bash
# inside:
ros2 launch omnidirectional_four_wheeled_robot gazebo.launch.py gui:=false
```

## Native build (ROS 2 Humble, Jazzy, or Rolling installed)

```bash
# Replace <distro> with humble, jazzy, or rolling to match your installed ROS 2.
sudo apt install \
  ros-<distro>-ros2-control ros-<distro>-ros2-controllers ros-<distro>-gz-ros2-control \
  ros-<distro>-ros-gz-sim ros-<distro>-ros-gz-bridge ros-<distro>-ros-gz-interfaces \
  ros-<distro>-robot-state-publisher ros-<distro>-joint-state-publisher-gui \
  ros-<distro>-xacro ros-<distro>-rviz2

mkdir -p ~/ros2_ws/src
ln -s <this-repo> ~/ros2_ws/src/omnidirectional_four_wheeled_robot
cd ~/ros2_ws && colcon build && source install/setup.bash
```

## Run

Full simulation (Gazebo GUI + controllers + rviz2):

```bash
ros2 launch omnidirectional_four_wheeled_robot gazebo.launch.py
# headless (server only):  ... gazebo.launch.py gui:=false rviz:=false
```

Drive the robot (omnidirectional: linear.x, linear.y, angular.z):

```bash
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist '{linear: {x: 0.5, y: 0.0}, angular: {z: 0.0}}'
```

Model-only visualisation (no Gazebo):

```bash
ros2 launch omnidirectional_four_wheeled_robot display.launch.py
```

## Topics / controllers

| Topic                            | Type                          | Note                          |
|----------------------------------|-------------------------------|-------------------------------|
| `/cmd_vel`                       | `geometry_msgs/Twist`         | command input                 |
| `/position_controller/commands`  | `std_msgs/Float64MultiArray`  | steering angles [FR,RR,FL,RL] |
| `/velocity_controller/commands`  | `std_msgs/Float64MultiArray`  | wheel speeds   [FR,RR,FL,RL]  |
| `/joint_states`                  | `sensor_msgs/JointState`      | joint_state_broadcaster       |
| `/odom`                          | `nav_msgs/Odometry`           | odometry_publisher (forward kinematics), also broadcasts `odom`->`base_link` TF |
| `/imu`                           | `sensor_msgs/Imu`             | bridged from gz               |
| `/camera/image`, `/camera/camera_info` | `sensor_msgs/Image`, `CameraInfo` | bridged from gz         |

Controllers (`config/controllers.yaml`): `joint_state_broadcaster`,
`position_controller` (JointGroupPositionController),
`velocity_controller` (JointGroupVelocityController).

```bash
ros2 control list_controllers   # all three should be "active"
```
