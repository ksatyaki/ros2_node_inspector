ARG ROS_DISTRO=humble
FROM osrf/ros:${ROS_DISTRO}-desktop

ARG USERNAME=ros
ARG USER_UID=1000
ARG USER_GID=1000

RUN apt-get update && apt-get install -y \
    build-essential cmake git sudo \
    python3-colcon-common-extensions \
    libglfw3-dev libgl1-mesa-dev \
  && rm -rf /var/lib/apt/lists/*

# Reclaim UID 1000 if a default user (e.g. 'ubuntu' on 24.04) already holds it.
RUN if getent passwd ${USER_UID} >/dev/null; then \
      userdel -r "$(getent passwd ${USER_UID} | cut -d: -f1)" 2>/dev/null || true; \
    fi; \
    groupadd --gid ${USER_GID} ${USERNAME} 2>/dev/null || true; \
    useradd  --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash ${USERNAME}; \
    echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME}; \
    chmod 0440 /etc/sudoers.d/${USERNAME}

ENV CMAKE_EXPORT_COMPILE_COMMANDS=1
USER ros
WORKDIR /home/ros/ws
