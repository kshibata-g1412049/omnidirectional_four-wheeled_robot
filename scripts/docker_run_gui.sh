#!/usr/bin/env bash
# ===========================================================================
#  docker_run_gui.sh - run gazebo.launch.py with GUI (gz + rviz2) in Docker,
#  forwarding the display to the host's X server via X11.
#
#  scripts/docker_build_and_test.sh / smoke_test.sh only exercise the
#  headless path (gui:=false rviz:=false) for CI. This script is for
#  interactively driving the simulation: it builds the image if needed,
#  grants the container temporary access to the host X server, and launches
#  gazebo.launch.py with the Gazebo and rviz2 windows on screen.
#
#  Requirements: Linux host with an X server (the usual case for desktop
#  Linux). Not supported as-is on macOS/Windows -- those need a third-party
#  X server (XQuartz / VcXsrv) and different docker run flags; see the
#  Windows section of README.md for the headless-only Docker workflow there.
#
#  Usage:
#    bash scripts/docker_run_gui.sh [image_tag] [ros_distro] [-- <extra ros2 launch args>]
#
#  Examples:
#    bash scripts/docker_run_gui.sh                          # omni4wd:jazzy, jazzy
#    bash scripts/docker_run_gui.sh omni4wd:humble humble
#    bash scripts/docker_run_gui.sh omni4wd:jazzy jazzy -- rviz:=false   # gz GUI only
# ===========================================================================
set -euo pipefail

ROS_DISTRO="${2:-${ROS_DISTRO:-jazzy}}"
IMAGE="${1:-omni4wd:${ROS_DISTRO}}"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

# Anything after a literal "--" is forwarded to `ros2 launch` as extra args
# (e.g. `rviz:=false` to show only the Gazebo window).
LAUNCH_ARGS=()
forward=0
for arg in "$@"; do
  if [ "$forward" -eq 1 ]; then
    LAUNCH_ARGS+=("$arg")
  elif [ "$arg" = "--" ]; then
    forward=1
  fi
done

if [ "$(uname -s)" != "Linux" ]; then
  echo "[run-gui] this script forwards X11 via /tmp/.X11-unix, which only" >&2
  echo "[run-gui] works on Linux hosts. See README.md for macOS/Windows options." >&2
  exit 1
fi

if [ -z "${DISPLAY:-}" ]; then
  echo "[run-gui] \$DISPLAY is not set -- are you in a graphical session?" >&2
  exit 1
fi

# Build the image if it doesn't exist yet (same build args as docker_build_and_test.sh).
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "[run-gui] image $IMAGE not found, building it first ..."
  docker build --build-arg "ROS_DISTRO=$ROS_DISTRO" -t "$IMAGE" "$REPO_DIR"
fi

# Grant the container temporary access to the host X server, and revoke it on exit.
echo "[run-gui] granting container access to the X server (xhost +local:docker) ..."
xhost +local:docker >/dev/null
cleanup() {
  echo "[run-gui] revoking X server access (xhost -local:docker) ..."
  xhost -local:docker >/dev/null 2>&1 || true
}
trap cleanup EXIT

# Pass the host GPU through if present, for hardware-accelerated rendering;
# otherwise fall back to LIBGL_ALWAYS_SOFTWARE (set below) on plain CPU rendering.
DEVICE_ARGS=()
if [ -e /dev/dri ]; then
  DEVICE_ARGS+=(--device /dev/dri:/dev/dri)
fi

echo "[run-gui] launching gazebo.launch.py (gui:=true rviz:=true) in $IMAGE ..."
docker run --rm -it \
  -e DISPLAY="$DISPLAY" \
  -e QT_QPA_PLATFORM=xcb \
  -e LIBGL_ALWAYS_SOFTWARE=1 \
  -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
  "${DEVICE_ARGS[@]}" \
  "$IMAGE" \
  ros2 launch omnidirectional_four_wheeled_robot gazebo.launch.py "${LAUNCH_ARGS[@]}"
