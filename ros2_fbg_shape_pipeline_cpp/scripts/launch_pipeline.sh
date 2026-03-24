#!/bin/bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Launch the FBG ROS 2 pipeline from either cache install (~/.cache/fbg_colcon/...) or local workspace install (./install).

Usage:
  bash ./scripts/launch_pipeline.sh [--tcp-host <host>] [--tcp-port <port>] [--workspace <path>]

Options:
  --tcp-host <host>   TCP host for interrogator/mock stream (default: 127.0.0.1)
  --tcp-port <port>   TCP port for interrogator/mock stream (default: 50012)
  --workspace <path>  Workspace root (default: current directory)
  -h, --help          Show this help text
USAGE
}

TCP_HOST="127.0.0.1"
TCP_PORT="50012"
WORKSPACE_DIR="$(pwd)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --tcp-host)
      [[ $# -ge 2 ]] || { echo "Error: --tcp-host requires a value." >&2; usage; exit 1; }
      TCP_HOST="$2"
      shift 2
      ;;
    --tcp-port)
      [[ $# -ge 2 ]] || { echo "Error: --tcp-port requires a value." >&2; usage; exit 1; }
      TCP_PORT="$2"
      shift 2
      ;;
    --workspace)
      [[ $# -ge 2 ]] || { echo "Error: --workspace requires a value." >&2; usage; exit 1; }
      WORKSPACE_DIR="$2"
      shift 2
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

WORKSPACE_DIR="$(cd "$WORKSPACE_DIR" && pwd)"
WORKSPACE_NAME="$(basename "$WORKSPACE_DIR")"
CACHE_INSTALL_SETUP="${HOME}/.cache/fbg_colcon/${WORKSPACE_NAME}/install/setup.bash"
LOCAL_INSTALL_SETUP="${WORKSPACE_DIR}/install/setup.bash"
SOURCE_NEEDLE_CONFIG="${WORKSPACE_DIR}/fbg_shape_pipeline_cpp/config/needle_config.txt"

if [[ -f "$CACHE_INSTALL_SETUP" ]]; then
  INSTALL_SETUP="$CACHE_INSTALL_SETUP"
elif [[ -f "$LOCAL_INSTALL_SETUP" ]]; then
  INSTALL_SETUP="$LOCAL_INSTALL_SETUP"
else
  cat >&2 <<ERR
Could not find either setup file:
  - $CACHE_INSTALL_SETUP
  - $LOCAL_INSTALL_SETUP

Build first with one of:
  bash ./scripts/build_workspace.sh --ros-distro humble
  bash ./scripts/build_workspace.sh --ros-distro humble --use-local-build-dirs
ERR
  exit 1
fi

# Avoid stale environment entries pointing at non-executable synced mounts.
unset AMENT_PREFIX_PATH COLCON_PREFIX_PATH CMAKE_PREFIX_PATH

# setup.bash may reference unset variables (e.g., COLCON_TRACE); temporarily disable nounset.
# shellcheck disable=SC1090
set +u
source "$INSTALL_SETUP"
set -u

# Prefer source-tree needle config when launching from repo, so config edits are picked up
# immediately without requiring reinstall.
if [[ -f "$SOURCE_NEEDLE_CONFIG" ]]; then
  export FBG_NEEDLE_CONFIG_FILE="$SOURCE_NEEDLE_CONFIG"
fi

echo "Using setup: ${INSTALL_SETUP}"
if [[ -n "${FBG_NEEDLE_CONFIG_FILE:-}" ]]; then
  echo "Using needle config: ${FBG_NEEDLE_CONFIG_FILE}"
fi
echo "Launching pipeline with tcp_host=${TCP_HOST} tcp_port=${TCP_PORT}"
ros2 launch fbg_shape_pipeline_cpp fbg_shape_pipeline.launch.py \
  tcp_host:="$TCP_HOST" tcp_port:="$TCP_PORT"
