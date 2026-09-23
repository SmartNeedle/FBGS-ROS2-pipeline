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
  --separate-shape        Use the separate shape node instead of fused processing.
  --full-frame-input      Use the full raw ROS topic for processing (comparison mode).
  -h, --help              Show this help text.
USAGE
}

INSTALL_SETUP=""
TCP_HOST="127.0.0.1"
TCP_PORT="50012"
BRIDGE_PORT="18944"
BRIDGE_IP="127.0.0.1"
BRIDGE_MODE="server"
FUSED_SHAPE="true"
COMPACT_TOPIC="/needle/fbg_sensor_frame"
SENSOR_INPUT_TOPIC="/needle/fbg_sensor_frame"

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
    --separate-shape)
      FUSED_SHAPE="false"
      shift
      ;;
    --full-frame-input)
      COMPACT_TOPIC=""
      SENSOR_INPUT_TOPIC="/needle/fbg_frame"
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
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export FBG_NEEDLE_CONFIG_FILE="${FBG_NEEDLE_CONFIG_FILE:-$ROOT/ros2_fbg_shape_pipeline_cpp/fbg_shape_pipeline_cpp/config/needle_config.txt}"
export LD_LIBRARY_PATH="$ROOT/OpenIGTLink-build/bin:$ROOT/OpenIGTLink-build/lib:${LD_LIBRARY_PATH:-}"

set +u
source "$INSTALL_SETUP"
set -u

ros2 launch ros2_smartneedle_adapter full_pipeline.launch.py \
  tcp_host:="$TCP_HOST" \
  tcp_port:="$TCP_PORT" \
  mode:="$BRIDGE_MODE" \
  bridge_ip:="$BRIDGE_IP" \
  bridge_port:="$BRIDGE_PORT" \
  fused_shape:="$FUSED_SHAPE" \
  compact_topic:="$COMPACT_TOPIC" \
  sensor_input_topic:="$SENSOR_INPUT_TOPIC"
