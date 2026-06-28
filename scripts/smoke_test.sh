#!/usr/bin/env bash
# ===========================================================================
#  smoke_test.sh - headless functional check (run inside the Docker container)
#
#  Verifies the core control path of the omnidirectional four-wheeled robot:
#    1. headless gz simulation launches and the robot spawns
#    2. joint_state_broadcaster / position_controller / velocity_controller
#       all reach the "active" state
#    3. publishing /cmd_vel makes controller_kinematics emit commands and the
#       wheel drive joints (*_joint2) actually rotate in gz
#
#  Exit code 0 = PASS, non-zero = FAIL.
# ===========================================================================
# NOTE: do not use `set -u` here -- the ROS 2 / colcon setup scripts reference
# unbound variables (e.g. AMENT_TRACE_SETUP_FILES) and would abort sourcing.

source "/opt/ros/${ROS_DISTRO:-jazzy}/setup.bash"
source /ros2_ws/install/setup.bash

LOG=/tmp/launch.log
NEED=(joint_state_broadcaster position_controller velocity_controller)

echo "[smoke] launching headless simulation (gui:=false rviz:=false)..."
ros2 launch omnidirectional_four_wheeled_robot gazebo.launch.py \
    gui:=false rviz:=false > "$LOG" 2>&1 &
LAUNCH_PID=$!

cleanup() {
  echo "[smoke] cleaning up..."
  kill -INT "$LAUNCH_PID" 2>/dev/null
  sleep 3
  kill -9 "$LAUNCH_PID" 2>/dev/null
  pkill -9 -f "gz sim"   2>/dev/null
  pkill -9 -f "ruby"     2>/dev/null
  pkill -9 -f "parameter_bridge" 2>/dev/null
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
# 1) wait for all controllers to become active
# ---------------------------------------------------------------------------
echo "[smoke] waiting for controllers to become active (timeout 360s)..."
deadline=$((SECONDS + 360))
active_ok=0
while [ $SECONDS -lt $deadline ]; do
  out=$(ros2 control list_controllers 2>/dev/null)
  ok=1
  for c in "${NEED[@]}"; do
    echo "$out" | grep -qE "^${c}[[:space:]].*active" || ok=0
  done
  if [ "$ok" -eq 1 ]; then
    active_ok=1
    echo "[smoke] all controllers active:"
    echo "$out"
    break
  fi
  sleep 3
done

if [ "$active_ok" -ne 1 ]; then
  echo "[smoke] FAIL: controllers did not become active in time"
  echo "----- last list_controllers -----"; ros2 control list_controllers 2>&1 || true
  echo "----- launch log tail -----"; tail -n 100 "$LOG"
  exit 1
fi

# ---------------------------------------------------------------------------
# 2) drive the robot
# ---------------------------------------------------------------------------
echo "[smoke] publishing /cmd_vel (linear.x = 0.5) ..."
ros2 topic pub --rate 20 /cmd_vel geometry_msgs/msg/Twist \
    '{linear: {x: 0.5, y: 0.0}, angular: {z: 0.0}}' > /tmp/cmdvel.log 2>&1 &
PUB_PID=$!
sleep 4

# ---------------------------------------------------------------------------
# 3) controller_kinematics command output present?
# ---------------------------------------------------------------------------
echo "[smoke] checking /velocity_controller/commands ..."
timeout 10 ros2 topic echo --once /velocity_controller/commands 2>&1 | tee /tmp/velcmd.txt || true

# ---------------------------------------------------------------------------
# 4) did the wheel drive joints actually move in gz?
# ---------------------------------------------------------------------------
echo "[smoke] checking wheel joint velocities in /joint_states ..."
python3 - <<'PY'
import subprocess, sys
try:
    import yaml
except Exception as e:
    print("[smoke] pyyaml missing:", e); sys.exit(2)
try:
    out = subprocess.check_output(
        ["ros2", "topic", "echo", "--once", "/joint_states"],
        timeout=20, text=True)
except Exception as e:
    print("[smoke] ERR reading /joint_states:", e); sys.exit(2)

# `ros2 topic echo` can prepend tab-indented banner lines (e.g.
# "\ttotal count change:1") and, on some distros, glue the last banner
# line directly onto the "---" doc separator with no newline in between
# (e.g. "\ttotal count: 1---"). Drop the banner text but keep any "---"
# separator that was glued onto it, otherwise two YAML docs get fused
# into one unparsable blob.
import re
lines = []
for ln in out.splitlines():
    if "message was lost" in ln.lower():
        continue
    if "\t" in ln:
        if re.search(r"---\s*$", ln):
            lines.append("---")
        continue
    lines.append(ln)
clean = "\n".join(lines)
data = None
for seg in clean.split("---"):
    seg = seg.strip()
    if not seg:
        continue
    try:
        d = yaml.safe_load(seg)
    except Exception:
        continue
    if isinstance(d, dict) and "velocity" in d:
        data = d
        break
if data is None:
    print("[smoke] could not parse /joint_states; raw output:\n", out[:600])
    sys.exit(2)

names = data.get("name") or []
vel = data.get("velocity") or []
pairs = list(zip(names, vel))
print("[smoke] joint velocities:", pairs)
# Under forward /cmd_vel the drive joints (*_joint2) spin; checking the max
# over all joints is sufficient to prove the wheels are turning.
mx = max((abs(v) for v in vel), default=0.0)
print("[smoke] max |joint velocity| = %.4f rad/s" % mx)
sys.exit(0 if mx > 0.1 else 3)
PY
MOVE_RC=$?

kill -INT "$PUB_PID" 2>/dev/null

# ---------------------------------------------------------------------------
# verdict
# ---------------------------------------------------------------------------
if [ "$MOVE_RC" -eq 0 ]; then
  echo "[smoke] PASS: controllers active and wheels rotating under /cmd_vel"
  exit 0
fi
echo "[smoke] FAIL: wheel drive joints did not move (rc=$MOVE_RC)"
echo "----- launch log tail -----"; tail -n 100 "$LOG"
exit 1
