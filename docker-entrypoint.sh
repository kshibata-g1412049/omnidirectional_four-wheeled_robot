#!/usr/bin/env bash
# Source the ROS 2 environment and the workspace overlay, then exec the command.
set -e
source /opt/ros/jazzy/setup.bash
source /ros2_ws/install/setup.bash
exec "$@"
