# omnidirectional_four_wheeled_robot (ROS 2)

ROS 2 (**Jazzy**) + **Gazebo (gz / Harmonic)** port of the omnidirectional
four-wheeled (swerve-drive) robot. Each wheel has an independent steering joint
(`*_joint1`, position controlled) and a drive joint (`*_joint2`, velocity
controlled). The `controller_kinematics` node converts a body twist on
`/cmd_vel` into per-wheel steering angles and rotation speeds.

> The original ROS 1 (catkin) package is kept unchanged in the repository root.
> This package is renamed with underscores (`omnidirectional_four_wheeled_robot`)
> because ROS 2 package names may not contain hyphens.

## Dependencies

```bash
sudo apt install \
  ros-jazzy-ros2-control ros-jazzy-ros2-controllers \
  ros-jazzy-gz-ros2-control \
  ros-jazzy-ros-gz-sim ros-jazzy-ros-gz-bridge ros-jazzy-ros-gz-interfaces \
  ros-jazzy-robot-state-publisher ros-jazzy-joint-state-publisher-gui \
  ros-jazzy-xacro ros-jazzy-rviz2
```

## Build

```bash
mkdir -p ~/ros2_ws/src
ln -s <this-repo>/ros2/omnidirectional_four_wheeled_robot ~/ros2_ws/src/
cd ~/ros2_ws
colcon build --packages-select omnidirectional_four_wheeled_robot
source install/setup.bash
```

## Run

Full simulation (Gazebo + controllers + rviz2):

```bash
ros2 launch omnidirectional_four_wheeled_robot gazebo.launch.py
# rviz off:  ros2 launch ... gazebo.launch.py rviz:=false
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

| Topic                            | Type                          | Note                         |
|----------------------------------|-------------------------------|------------------------------|
| `/cmd_vel`                       | `geometry_msgs/Twist`         | command input                |
| `/position_controller/commands`  | `std_msgs/Float64MultiArray`  | steering angles [FR,RR,FL,RL]|
| `/velocity_controller/commands`  | `std_msgs/Float64MultiArray`  | wheel speeds   [FR,RR,FL,RL] |
| `/joint_states`                  | `sensor_msgs/JointState`      | joint_state_broadcaster      |
| `/imu`                           | `sensor_msgs/Imu`             | bridged from gz              |
| `/camera/image`, `/camera/camera_info` | `sensor_msgs/Image`, `CameraInfo` | bridged from gz        |

Controllers (see `config/controllers.yaml`): `joint_state_broadcaster`,
`position_controller` (JointGroupPositionController),
`velocity_controller` (JointGroupVelocityController).

```bash
ros2 control list_controllers   # all three should be "active"
```

## What changed from the ROS 1 version

- `roscpp` → `rclcpp`; the inverse-kinematics math is unchanged.
- Output changed from 8 individual `std_msgs/Float64` topics to 2
  `std_msgs/Float64MultiArray` topics consumed by the ros2_control JointGroup
  controllers.
- `ros_control` + `<transmission>` tags → `ros2_control` with the
  `gz_ros2_control/GazeboSimSystem` hardware and `controllers.yaml`.
- Gazebo Classic plugins → gz systems/sensors:
  `libgazebo_ros_control.so` → `gz_ros2_control-system`;
  IMU/camera/lidar → gz `<sensor>` + `ros_gz_bridge`.
- ROS 1 XML launch → Python launch (`*.launch.py`).
- `SpawnerSphere.cpp` hard-coded path → `spawner_sphere.cpp` resolves the model
  via `ament_index` and spawns through the `ros_gz_sim` create service
  (or via `ros_gz_sim create` in the launch file). The previously missing
  `sphere.urdf` is now included.
