#!/bin/bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Install Linux dependencies for CPP/ros2_fbg_shape_pipeline_cpp.

Usage:
  bash ./install_linux_dependencies.sh [--ros-distro <name>] [--skip-rosdep-init]

Options:
  --ros-distro <name>   ROS 2 distro to target (e.g. humble, jazzy).
                        If omitted, the script auto-detects from /opt/ros,
                        preferring jazzy then humble when both exist.
  --skip-rosdep-init    Skip `sudo rosdep init` / `rosdep update`.
                        Useful when rosdep is already initialized.
  -h, --help            Show this help text.
USAGE
}

ROS_DISTRO_ARG=""
SKIP_ROSDEP_INIT=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ros-distro)
      if [[ $# -lt 2 ]]; then
        echo "Error: --ros-distro requires a value." >&2
        usage
        exit 1
      fi
      ROS_DISTRO_ARG="$2"
      shift 2
      ;;
    --skip-rosdep-init)
      SKIP_ROSDEP_INIT=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Error: unknown option '$1'." >&2
      usage
      exit 1
      ;;
  esac
done

if [[ -n "$ROS_DISTRO_ARG" ]]; then
  ROS_DISTRO="$ROS_DISTRO_ARG"
else
  if [[ -d /opt/ros/jazzy ]]; then
    ROS_DISTRO="jazzy"
  elif [[ -d /opt/ros/humble ]]; then
    ROS_DISTRO="humble"
  elif [[ -n "${ROS_DISTRO:-}" ]]; then
    ROS_DISTRO="$ROS_DISTRO"
  else
    echo "Could not auto-detect ROS 2 distro. Re-run with --ros-distro <name>." >&2
    exit 1
  fi
fi

echo "Using ROS 2 distro: $ROS_DISTRO"

echo "[1/4] Installing apt dependencies..."
sudo apt update
sudo apt install -y \
  build-essential cmake git \
  python3 python3-pip \
  python3-colcon-common-extensions \
  python3-rosdep python3-vcstool \
  libboost-system-dev \
  "ros-${ROS_DISTRO}-ros-base" \
  "ros-${ROS_DISTRO}-ament-cmake" \
  "ros-${ROS_DISTRO}-rclcpp" \
  "ros-${ROS_DISTRO}-std-msgs" \
  "ros-${ROS_DISTRO}-geometry-msgs" \
  "ros-${ROS_DISTRO}-builtin-interfaces" \
  "ros-${ROS_DISTRO}-rosidl-default-generators" \
  "ros-${ROS_DISTRO}-rosidl-default-runtime" \
  "ros-${ROS_DISTRO}-launch" \
  "ros-${ROS_DISTRO}-launch-ros" \
  "ros-${ROS_DISTRO}-ament-index-python" \
  "ros-${ROS_DISTRO}-rclpy"
sudo apt install -y "ros-${ROS_DISTRO}-tf2-ros" "ros-${ROS_DISTRO}-ament-cmake-gtest"

if [[ "$SKIP_ROSDEP_INIT" -eq 0 ]]; then
  echo "[2/4] Initializing rosdep (idempotent)..."
  if sudo rosdep init 2>/tmp/rosdep_init_err.log; then
    echo "rosdep initialized."
  else
    if grep -q "already initialized" /tmp/rosdep_init_err.log; then
      echo "rosdep already initialized; continuing."
    else
      cat /tmp/rosdep_init_err.log >&2
      exit 1
    fi
  fi

  rosdep update
else
  echo "[2/4] Skipping rosdep init/update by request."
fi

echo "[3/4] Dependency installation complete."

echo "[4/4] Next steps for collaborators:"
cat <<NEXT
  1) Build using the OneDrive/synced-folder-safe helper:
       bash ./scripts/build_workspace.sh --ros-distro ${ROS_DISTRO}

  2) Source the resulting workspace:
       source $HOME/.cache/fbg_colcon/$(basename "$PWD")/install/setup.bash

     Optional for local Linux filesystems (in-workspace build dirs):
       bash ./scripts/build_workspace.sh --ros-distro ${ROS_DISTRO} --use-local-build-dirs

  3) Launch (mock example):
       bash ./scripts/launch_pipeline.sh --tcp-host 127.0.0.1 --tcp-port 50012
NEXT
