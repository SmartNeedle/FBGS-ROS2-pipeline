#!/bin/bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Launch the ROS 2 to 3D Slicer integration bridge from the CPP folder.

Usage:
  bash ./scripts/launch_slicer_bridge.sh --install-setup <path> [--input-topic <topic>] [--bridge-port <port>]

Options:
  --install-setup <path>  Path to the merged install/setup.bash produced by build_cpp_ros2_stack.sh.
  --input-topic <topic>   ROS 2 PoseArray topic to forward (default: /needle/state/current_shape).
  --bridge-port <port>    OpenIGTLink port for Slicer connection (default: 18944).
  --point-scale <value>   Scale applied to ROS positions before sending to Slicer (default: 1000.0).
  --reverse-point-order   Reverse the point order before publishing to Slicer.
  --invert-x              Negate the X coordinate.
  --invert-y              Negate the Y coordinate.
  --invert-z              Negate the Z coordinate.
  -h, --help              Show this help text.
USAGE
}

INSTALL_SETUP=""
INPUT_TOPIC="/needle/state/current_shape"
BRIDGE_PORT="18944"
POINT_SCALE="1000.0"
REVERSE_POINT_ORDER="false"
INVERT_X="false"
INVERT_Y="false"
INVERT_Z="false"

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
    --point-scale)
      [[ $# -ge 2 ]] || { echo "Error: --point-scale requires a value." >&2; usage; exit 1; }
      POINT_SCALE="$2"
      shift 2
      ;;
    --reverse-point-order)
      REVERSE_POINT_ORDER="true"
      shift
      ;;
    --invert-x)
      INVERT_X="true"
      shift
      ;;
    --invert-y)
      INVERT_Y="true"
      shift
      ;;
    --invert-z)
      INVERT_Z="true"
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

set +u
source "$INSTALL_SETUP"
set -u

ros2 launch fbg_slicer_bridge needle_to_slicer_bridge.launch.py \
  input_topic:="$INPUT_TOPIC" \
  bridge_port:="$BRIDGE_PORT" \
  point_scale:="$POINT_SCALE" \
  reverse_point_order:="$REVERSE_POINT_ORDER" \
  invert_x:="$INVERT_X" \
  invert_y:="$INVERT_Y" \
  invert_z:="$INVERT_Z"
