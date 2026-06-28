# ===========================================================================
#  Dockerfile - omnidirectional_four_wheeled_robot (ROS 2 Jazzy + gz Harmonic)
#
#  Builds a colcon workspace containing this package and all runtime
#  dependencies (ros2_control, gz_ros2_control, ros_gz). Suitable for the
#  headless smoke test in scripts/smoke_test.sh as well as interactive use.
#
#  Build (normal):
#    docker build -t omni4wd:jazzy .
#  Run the smoke test:
#    docker run --rm omni4wd:jazzy smoke_test.sh
#  Interactive shell:
#    docker run --rm -it omni4wd:jazzy bash
# ===========================================================================
FROM ros:jazzy-ros-base

# Optional HTTP(S) proxy support for restricted build networks.
ARG http_proxy=""
ARG https_proxy=""
ARG no_proxy=""
ENV http_proxy=${http_proxy} \
    https_proxy=${https_proxy} \
    no_proxy=${no_proxy} \
    HTTP_PROXY=${http_proxy} \
    HTTPS_PROXY=${https_proxy} \
    NO_PROXY=${no_proxy}

# Optional extra CA certificate (e.g. a TLS-terminating egress proxy).
# Provide build context file ca-bundle.crt to enable; otherwise this is a no-op.
COPY ca-bundle.cr[t] /usr/local/share/ca-certificates/extra-ca.crt
RUN if [ -s /usr/local/share/ca-certificates/extra-ca.crt ]; then \
        update-ca-certificates; \
    else \
        rm -f /usr/local/share/ca-certificates/extra-ca.crt; \
    fi

ENV DEBIAN_FRONTEND=noninteractive

# Runtime / simulation dependencies. ros-gz-sim pulls in gz (Harmonic).
RUN apt-get update && apt-get install -y --no-install-recommends \
        ros-jazzy-ros-gz-sim \
        ros-jazzy-ros-gz-bridge \
        ros-jazzy-ros-gz-interfaces \
        ros-jazzy-gz-ros2-control \
        ros-jazzy-ros2-control \
        ros-jazzy-ros2-controllers \
        ros-jazzy-xacro \
        ros-jazzy-robot-state-publisher \
        ros-jazzy-joint-state-publisher-gui \
        ros-jazzy-rviz2 \
        libgl1-mesa-dri \
        libegl1 \
        python3-yaml \
    && rm -rf /var/lib/apt/lists/*

# Build the workspace.
WORKDIR /ros2_ws
COPY . /ros2_ws/src/omnidirectional_four_wheeled_robot
RUN . /opt/ros/jazzy/setup.sh \
    && colcon build \
    && rm -rf build log

# Make the smoke test directly invocable and source the overlay on each shell.
RUN cp /ros2_ws/src/omnidirectional_four_wheeled_robot/scripts/smoke_test.sh /usr/local/bin/smoke_test.sh \
    && chmod +x /usr/local/bin/smoke_test.sh \
    && echo 'source /opt/ros/jazzy/setup.bash' >> /root/.bashrc \
    && echo 'source /ros2_ws/install/setup.bash' >> /root/.bashrc

# Headless / offscreen defaults.
ENV QT_QPA_PLATFORM=offscreen \
    LIBGL_ALWAYS_SOFTWARE=1

COPY docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
RUN chmod +x /usr/local/bin/docker-entrypoint.sh
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
CMD ["bash"]
