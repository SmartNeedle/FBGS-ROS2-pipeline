import os
import re

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _parse_numeric_list(raw_value: str):
    cleaned = raw_value.strip()
    if not cleaned:
        return []
    tokens = re.findall(r"[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?", cleaned)
    return [float(token) for token in tokens]


def _parse_first_numeric(raw_value: str):
    values = _parse_numeric_list(raw_value)
    if not values:
        raise ValueError(f"No numeric value found in '{raw_value}'")
    return values[0]


def _normalize_key(raw_key: str):
    return re.sub(r"[^a-z0-9]", "", raw_key.lower())


def _load_needle_config(config_path: str):
    if not os.path.exists(config_path):
        return {}

    parsed = {}
    with open(config_path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.split("#", 1)[0].strip()
            if not line:
                continue

            if "=" in line:
                key, value = line.split("=", 1)
            elif ":" in line:
                key, value = line.split(":", 1)
            else:
                continue

            key = _normalize_key(key.strip())
            value = value.strip()
            parsed[key] = value

    out = {}
    key_aliases = {
        "needle_length_mm": ["needlelengthmm", "needlelengthm", "needlelength", "needletotallengthmm", "needletotallengthm", "needletotallength", "totallengthmm", "totallengthm"],
        "first_fbg_index": ["firstfbgindex", "firstfbg", "firstincludedfbgindex"],
        "sensor_arc_lengths_mm": ["sensorarclengthsmm", "sensorarclengthsm", "sensorarclengths", "arclengths"],
        "curvature_scale": ["curvaturescale", "curvaturescales"],
        "orientation_sign": ["orientationsign", "orientationsigns"],
        "orientation_offset_rad": ["orientationoffsetrad", "orientationoffset"],
    }

    for canonical_key, aliases in key_aliases.items():
        for alias in aliases:
            if alias in parsed:
                raw_value = parsed[alias]
                if canonical_key in ("needle_length_mm",):
                    out[canonical_key] = float(_parse_first_numeric(raw_value))
                elif canonical_key in ("first_fbg_index",):
                    out[canonical_key] = int(_parse_first_numeric(raw_value))
                else:
                    out[canonical_key] = _parse_numeric_list(raw_value)
                break

    if not out:
        print(
            f"[fbg_shape_pipeline.launch] Found {config_path} but no known keys were parsed. "
            "Expected keys like needle_length_mm, first_fbg_index, sensor_arc_lengths_mm, "
            "curvature_scale, orientation_sign, orientation_offset_rad."
        )

    first_fbg_index = max(1, int(out.get("first_fbg_index", 1)))
    out["first_fbg_index"] = first_fbg_index

    vector_keys = [k for k in ("sensor_arc_lengths_mm", "curvature_scale", "orientation_sign", "orientation_offset_rad") if k in out]
    if vector_keys:
        lengths = {k: len(out[k]) for k in vector_keys}
        if len(set(lengths.values())) != 1:
            raise ValueError(
                "needle_config.txt produced mismatched vector lengths: "
                + ", ".join(f"{k}={v}" for k, v in lengths.items())
            )

    return out


def generate_launch_description():
    tcp_host = LaunchConfiguration("tcp_host")
    tcp_port = LaunchConfiguration("tcp_port")
    package_share = get_package_share_directory("fbg_shape_pipeline_cpp")
    config_file = os.path.join(package_share, "config", "pipeline.yaml")
    env_needle_config_file = os.environ.get("FBG_NEEDLE_CONFIG_FILE", "").strip()
    needle_config_file = env_needle_config_file or os.path.join(package_share, "config", "needle_config.txt")
    needle_overrides = _load_needle_config(needle_config_file)
    if env_needle_config_file:
        print(f"[fbg_shape_pipeline.launch] Using FBG_NEEDLE_CONFIG_FILE={needle_config_file}")
    if needle_overrides:
        print(f"[fbg_shape_pipeline.launch] Loaded needle_config overrides from {needle_config_file}")
        print(f"[fbg_shape_pipeline.launch] Override keys: {sorted(needle_overrides.keys())}")

    curvature_overrides = {}
    shape_overrides = {}
    for key in ("first_fbg_index", "sensor_arc_lengths_mm", "curvature_scale", "orientation_sign", "orientation_offset_rad"):
        if key in needle_overrides:
            curvature_overrides[key] = needle_overrides[key]
    if "needle_length_mm" in needle_overrides:
        shape_overrides["needle_length_mm"] = needle_overrides["needle_length_mm"]

    return LaunchDescription([
        DeclareLaunchArgument("tcp_host", default_value="127.0.0.1"),
        DeclareLaunchArgument("tcp_port", default_value="50012"),
        Node(
            package="fbg_shape_pipeline_cpp",
            executable="tcp_receiver_node",
            name="tcp_receiver_node",
            output="screen",
            parameters=[config_file, {"tcp_host": tcp_host, "tcp_port": tcp_port}],
        ),
        Node(
            package="fbg_shape_pipeline_cpp",
            executable="curvature_processor_node",
            name="curvature_processor_node",
            output="screen",
            parameters=[config_file, curvature_overrides],
        ),
        Node(
            package="fbg_shape_pipeline_cpp",
            executable="shape_publisher_node",
            name="shape_publisher_node",
            output="screen",
            parameters=[config_file, shape_overrides],
        ),
    ])
