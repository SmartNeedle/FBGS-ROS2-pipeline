#!/bin/bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Build the ROS 2 workspace in this folder.

This script defaults to build/install/log directories under $HOME/.cache
to avoid symlink/noexec issues on OneDrive/synced mounts.

Usage:
  bash ./scripts/build_workspace.sh [--ros-distro <name>] [--workspace <path>] [--use-local-build-dirs] [--no-clean]

Options:
  --ros-distro <name>      ROS 2 distro to source from /opt/ros/<name>.
                           If omitted, auto-detects jazzy/humble or uses $ROS_DISTRO.
  --workspace <path>       Workspace root to build (default: current directory).
  --use-local-build-dirs   Store build/install/log under workspace (standard colcon behavior).
                           Default behavior uses $HOME/.cache/fbg_colcon/<workspace_name>/.
  --no-clean               Keep existing build/install/log folders (incremental build).
                           Default behavior is a clean rebuild for reliability.
  -h, --help               Show this help text.
USAGE
}

ROS_DISTRO_ARG=""
WORKSPACE_DIR="$(pwd)"
USE_LOCAL_BUILD_DIRS=0
NO_CLEAN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
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
    --use-local-build-dirs)
      USE_LOCAL_BUILD_DIRS=1
      shift
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
WORKSPACE_NAME="$(basename "$WORKSPACE_DIR")"

if [[ "$USE_LOCAL_BUILD_DIRS" -eq 1 ]]; then
  BUILD_BASE="${WORKSPACE_DIR}/build"
  INSTALL_BASE="${WORKSPACE_DIR}/install"
  LOG_BASE="${WORKSPACE_DIR}/log"
else
  CACHE_ROOT="${HOME}/.cache/fbg_colcon/${WORKSPACE_NAME}"
  BUILD_BASE="${CACHE_ROOT}/build"
  INSTALL_BASE="${CACHE_ROOT}/install"
  LOG_BASE="${CACHE_ROOT}/log"
fi

if [[ "$NO_CLEAN" -eq 0 ]]; then
  echo "Cleaning previous build/install/log folders for a reliable rebuild..."
  rm -rf "$BUILD_BASE" "$INSTALL_BASE" "$LOG_BASE"
fi

mkdir -p "$BUILD_BASE" "$INSTALL_BASE" "$LOG_BASE"

# ROS setup scripts may reference unset variables; temporarily disable nounset.
# shellcheck disable=SC1090
set +u
source "/opt/ros/${ROS_DISTRO}/setup.bash"
set -u

echo "Using ROS 2 distro: ${ROS_DISTRO}"
echo "Workspace: ${WORKSPACE_DIR}"
echo "Build base: ${BUILD_BASE}"
echo "Install base: ${INSTALL_BASE}"
echo "Log base: ${LOG_BASE}"

echo "Running colcon build..."
# Note: --log-base is a global colcon option and must appear before the verb.
colcon --log-base "$LOG_BASE" build \
  --base-paths "$WORKSPACE_DIR" \
  --build-base "$BUILD_BASE" \
  --install-base "$INSTALL_BASE" \
  --merge-install

echo "Build complete. Source with:"
echo "  source ${INSTALL_BASE}/setup.bash"
