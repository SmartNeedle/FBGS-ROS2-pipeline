#!/bin/bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Launch the canonical ROS 2 to 3D Slicer bridge from the repository root.

Usage:
  bash ./scripts/launch_slicer_bridge.sh --install-setup <path> [--input-topic <topic>] [--bridge-port <port>]

Options:
  --install-setup <path>  Path to the merged install/setup.bash produced by build_cpp_ros2_stack.sh.
  --input-topic <topic>   ROS 2 PoseArray topic to forward (default: /needle/state/current_shape).
  --bridge-port <port>    OpenIGTLink port for Slicer connection (default: 18944).
  --bridge-ip <value>     OpenIGTLink host/IP for client mode (default: 127.0.0.1).
  --bridge-mode <value>   OpenIGTLink mode: server or client (default: server).
  -h, --help              Show this help text.
USAGE
}

INSTALL_SETUP=""
INPUT_TOPIC="/needle/state/current_shape"
BRIDGE_PORT="18944"
BRIDGE_IP="127.0.0.1"
BRIDGE_MODE="server"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install-setup)
      [[ $# -ge 2 ]] || { echo "Error: --install-setup requires a value." >&2; usage; exit 1; }
      INSTALL_SETUP="$2"
      shift 2
      ;;
    --input-topic)
      [[ $# -ge 2 ]] || { echo "Error: --input-topic requires a value." >&2; usage; exit 1; }
      INPUT_TOPIC="$2"
      shift 2
      ;;
    --bridge-port)
      [[ $# -ge 2 ]] || { echo "Error: --bridge-port requires a value." >&2; usage; exit 1; }
      BRIDGE_PORT="$2"
      shift 2
      ;;
    --bridge-ip)
      [[ $# -ge 2 ]] || { echo "Error: --bridge-ip requires a value." >&2; usage; exit 1; }
      BRIDGE_IP="$2"
      shift 2
      ;;
    --bridge-mode)
      [[ $# -ge 2 ]] || { echo "Error: --bridge-mode requires a value." >&2; usage; exit 1; }
      BRIDGE_MODE="$2"
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

if [[ -z "$INSTALL_SETUP" ]]; then
  echo "Error: --install-setup is required." >&2
  usage
  exit 1
fi

INSTALL_SETUP="$(cd "$(dirname "$INSTALL_SETUP")" && pwd)/$(basename "$INSTALL_SETUP")"
if [[ ! -f "$INSTALL_SETUP" ]]; then
  echo "Missing install setup file: $INSTALL_SETUP" >&2
  exit 1
fi

unset AMENT_PREFIX_PATH COLCON_PREFIX_PATH CMAKE_PREFIX_PATH
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export LD_LIBRARY_PATH="$ROOT/OpenIGTLink-build/bin:$ROOT/OpenIGTLink-build/lib:${LD_LIBRARY_PATH:-}"

set +u
source "$INSTALL_SETUP"
set -u

ros2 launch ros2_smartneedle_adapter slicer_bridge.launch.py \
  mode:="$BRIDGE_MODE" \
  port:="$BRIDGE_PORT" \
  ip:="$BRIDGE_IP" \
  input_topic:="$INPUT_TOPIC"

