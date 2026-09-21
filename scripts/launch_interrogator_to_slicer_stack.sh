#!/bin/bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Launch the full interrogator-to-Slicer stack from the repository root.

Usage:
  bash ./scripts/launch_interrogator_to_slicer_stack.sh --install-setup <path> [options]

Options:
  --install-setup <path>  Path to the merged install/setup.bash produced by build_cpp_ros2_stack.sh.
  --tcp-host <value>      TCP host for the interrogator stream (default: 127.0.0.1).
  --tcp-port <value>      TCP port for the interrogator stream (default: 50012).
  --bridge-port <value>   OpenIGTLink port exposed to Slicer (default: 18944).
  --bridge-ip <value>     OpenIGTLink IP/host for client mode (default: 127.0.0.1).
  --bridge-mode <value>   OpenIGTLink mode: server or client (default: server).
  --point-scale <value>   Scale applied before sending points to Slicer (default: 1000.0).
  --output-frame-id <id>  Frame id encoded into the OpenIGTLink header (default: zFrame).
  --reverse-point-order   Reverse the point order before publishing.
  --invert-x              Negate the X coordinate.
  --invert-y              Negate the Y coordinate.
  --invert-z              Negate the Z coordinate.
  -h, --help              Show this help text.
USAGE
}

INSTALL_SETUP=""
TCP_HOST="127.0.0.1"
TCP_PORT="50012"
BRIDGE_PORT="18944"
BRIDGE_IP="127.0.0.1"
BRIDGE_MODE="server"
POINT_SCALE="1000.0"
OUTPUT_FRAME_ID="zFrame"
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
    --point-scale)
      [[ $# -ge 2 ]] || { echo "Error: --point-scale requires a value." >&2; usage; exit 1; }
      POINT_SCALE="$2"
      shift 2
      ;;
    --output-frame-id)
      [[ $# -ge 2 ]] || { echo "Error: --output-frame-id requires a value." >&2; usage; exit 1; }
      OUTPUT_FRAME_ID="$2"
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

ros2 launch ros2_smartneedle_adapter full_pipeline.launch.py \
  tcp_host:="$TCP_HOST" \
  tcp_port:="$TCP_PORT" \
  mode:="$BRIDGE_MODE" \
  bridge_ip:="$BRIDGE_IP" \
  bridge_port:="$BRIDGE_PORT" \
  point_scale:="$POINT_SCALE" \
  output_frame_id:="$OUTPUT_FRAME_ID" \
  reverse_point_order:="$REVERSE_POINT_ORDER" \
  invert_x:="$INVERT_X" \
  invert_y:="$INVERT_Y" \
  invert_z:="$INVERT_Z"

