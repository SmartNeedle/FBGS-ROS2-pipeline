#!/bin/bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Build the ROS 2 + Slicer integration stack from the CPP folder.

Usage:
  bash ./scripts/build_cpp_ros2_stack.sh --openigtlink-dir <path> [--ros-distro <name>] [--workspace <path>] [--no-clean]

Options:
  --openigtlink-dir <path>  CMake directory containing OpenIGTLinkConfig.cmake.
  --ros-distro <name>       ROS 2 distro to source from /opt/ros/<name>.
                            If omitted, auto-detects jazzy/humble or uses $ROS_DISTRO.
  --workspace <path>        Workspace root to build (default: current directory).
  --no-clean                Keep existing build/install/log folders.
  -h, --help                Show this help text.
USAGE
}

ROS_DISTRO_ARG=""
WORKSPACE_DIR="$(pwd)"
OPENIGTL_DIR=""
NO_CLEAN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --openigtlink-dir)
      [[ $# -ge 2 ]] || { echo "Error: --openigtlink-dir requires a value." >&2; usage; exit 1; }
      OPENIGTL_DIR="$2"
      shift 2
      ;;
    --ros-distro)
      [[ $# -ge 2 ]] || { echo "Error: --ros-distro requires a value." >&2; usage; exit 1; }
      ROS_DISTRO_ARG="$2"
      shift 2
      ;;
    --workspace)
      [[ $# -ge 2 ]] || { echo "Error: --workspace requires a value." >&2; usage; exit 1; }
      WORKSPACE_DIR="$2"
      shift 2
      ;;
    --no-clean)
      NO_CLEAN=1
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

if [[ -z "$OPENIGTL_DIR" ]]; then
  echo "Error: --openigtlink-dir is required." >&2
  usage
  exit 1
fi

if [[ -n "$ROS_DISTRO_ARG" ]]; then
  ROS_DISTRO="$ROS_DISTRO_ARG"
elif [[ -d /opt/ros/jazzy ]]; then
  ROS_DISTRO="jazzy"
elif [[ -d /opt/ros/humble ]]; then
  ROS_DISTRO="humble"
elif [[ -n "${ROS_DISTRO:-}" ]]; then
  ROS_DISTRO="$ROS_DISTRO"
else
  echo "Could not auto-detect ROS 2 distro. Re-run with --ros-distro <name>." >&2
  exit 1
fi

if [[ ! -f "/opt/ros/${ROS_DISTRO}/setup.bash" ]]; then
  echo "Missing /opt/ros/${ROS_DISTRO}/setup.bash. Install ROS first." >&2
  exit 1
fi

WORKSPACE_DIR="$(cd "$WORKSPACE_DIR" && pwd)"
OPENIGTL_DIR="$(cd "$OPENIGTL_DIR" && pwd)"
WORKSPACE_NAME="$(basename "$WORKSPACE_DIR")"
CACHE_ROOT="${HOME}/.cache/fbg_colcon/${WORKSPACE_NAME}_slicer"
BUILD_BASE="${CACHE_ROOT}/build"
INSTALL_BASE="${CACHE_ROOT}/install"
LOG_BASE="${CACHE_ROOT}/log"

if [[ "$NO_CLEAN" -eq 0 ]]; then
  rm -rf "$BUILD_BASE" "$INSTALL_BASE" "$LOG_BASE"
fi

mkdir -p "$BUILD_BASE" "$INSTALL_BASE" "$LOG_BASE"

set +u
source "/opt/ros/${ROS_DISTRO}/setup.bash"
set -u

echo "Using ROS 2 distro: ${ROS_DISTRO}"
echo "Workspace: ${WORKSPACE_DIR}"
echo "OpenIGTLink_DIR: ${OPENIGTL_DIR}"
echo "Install base: ${INSTALL_BASE}"

colcon --log-base "$LOG_BASE" build \
  --base-paths "$WORKSPACE_DIR/ros2_fbg_shape_pipeline_cpp" "$WORKSPACE_DIR/ros2_igtl_bridge" "$WORKSPACE_DIR/fbg_slicer_bridge" \
  --build-base "$BUILD_BASE" \
  --install-base "$INSTALL_BASE" \
  --merge-install \
  --cmake-args "-DOpenIGTLink_DIR=${OPENIGTL_DIR}"

echo "Build complete. Source with:"
echo "  source ${INSTALL_BASE}/setup.bash"
