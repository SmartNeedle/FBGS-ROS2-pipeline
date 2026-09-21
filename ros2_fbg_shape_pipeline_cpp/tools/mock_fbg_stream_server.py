#!/usr/bin/env python3
import argparse
import math
from pathlib import Path
import runpy
import socket
import struct
import time


def pack_field(field_id: int, payload: bytes) -> bytes:
    field_length = 2 + len(payload)
    return struct.pack("<IH", field_length, field_id) + payload


def make_packet(sample_index: int, active_areas: int, rate_hz: float = 100.0) -> bytes:
    timestamp = time.time()
    fiber_index = 0

    curvature_x = []
    curvature_y = []
    temperature = []
    for i in range(active_areas):
        phase = (sample_index / rate_hz) * 5.0 + i * 0.3
        # The interrogator curvature units are 1/mm. These amplitudes remain
        # 0.002..0.003 1/mm equals 2..3 1/m, below the requested 4 1/m.
        curvature_x.append(0.003 * math.sin(phase))
        curvature_y.append(0.002 * math.cos(phase))
        temperature.append(22.0 + 0.1 * math.sin(phase))

    curvature = []
    angle = []
    for i in range(active_areas):
        curvature.append(math.sqrt(curvature_x[i] ** 2 + curvature_y[i] ** 2))
        angle.append(math.atan2(curvature_y[i], curvature_x[i]))

    shape_points = []
    for step in range(20):
        z = step * 0.001
        x = 0.001 * math.sin(sample_index * 0.02) * step
        y = 0.001 * math.cos(sample_index * 0.02) * step
        shape_points.extend([x, y, z])

    payload = struct.pack("<B", fiber_index)
    payload += pack_field(0, struct.pack("<H", 0))
    payload += pack_field(1, struct.pack("<Q", sample_index))
    payload += pack_field(2, struct.pack("<d", timestamp))
    payload += pack_field(
        3,
        struct.pack("<I", len(curvature)) + struct.pack(f"<{len(curvature)}f", *curvature),
    )
    payload += pack_field(
        4,
        struct.pack("<I", len(angle)) + struct.pack(f"<{len(angle)}f", *angle),
    )
    payload += pack_field(
        5,
        struct.pack("<II", 3, len(shape_points) // 3) +
        struct.pack(f"<{len(shape_points)}f", *shape_points),
    )
    payload += pack_field(
        6,
        struct.pack("<I", len(temperature)) + struct.pack(f"<{len(temperature)}f", *temperature),
    )

    spectra_payload = b""
    for channel in range(min(active_areas, 4)):
        spectrum_wl = [1550000 + channel * 10 + j for j in range(8)]
        spectrum_power = [1000 + j for j in range(8)]
        peaks_wl = [1550000 + channel * 10 + j * 20 for j in range(2)]
        peaks_power = [2000 + j for j in range(2)]

        core_payload = struct.pack("<B", channel)
        core_payload += pack_field(0, struct.pack("<d", timestamp))
        core_payload += pack_field(1, struct.pack("<Q", sample_index))
        core_payload += pack_field(2, struct.pack("<H", 0))
        core_payload += pack_field(
            3,
            struct.pack("<I", len(spectrum_wl)) + struct.pack(f"<{len(spectrum_wl)}I", *spectrum_wl),
        )
        core_payload += pack_field(
            4,
            struct.pack("<I", len(spectrum_power)) +
            struct.pack(f"<{len(spectrum_power)}H", *spectrum_power),
        )
        core_payload += pack_field(
            5,
            struct.pack("<I", len(peaks_wl)) + struct.pack(f"<{len(peaks_wl)}I", *peaks_wl),
        )
        core_payload += pack_field(
            6,
            struct.pack("<I", len(peaks_power)) + struct.pack(f"<{len(peaks_power)}H", *peaks_power),
        )
        core_payload += pack_field(7, struct.pack("<I", 2500 + channel))
        spectra_payload += struct.pack("<I", len(core_payload) + 4) + core_payload

    payload += pack_field(7, spectra_payload)

    return struct.pack("<I", len(payload)) + payload


def main():
    parser = argparse.ArgumentParser(description="Mock FBG TCP stream server.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=50012)
    parser.add_argument("--rate-hz", type=float, default=100.0)
    parser.add_argument("--active-areas", type=int, default=None)
    config_dir = Path(__file__).resolve().parents[1] / "fbg_shape_pipeline_cpp" / "config"
    parser.add_argument("--needle-config", type=Path, default=config_dir / "needle_config.txt")
    args = parser.parse_args()
    if not math.isfinite(args.rate_hz) or args.rate_hz <= 0 or not 1 <= args.port <= 65535:
        parser.error("rate-hz must be positive and finite; port must be 1..65535")
    calibration = runpy.run_path(str(config_dir / "needle_calibration.py"))["load_needle_config"](args.needle_config)
    required = calibration["first_fbg_index"] - 1 + len(calibration["sensor_arc_lengths_mm"])
    if args.active_areas is None:
        args.active_areas = required
    if args.active_areas < required:
        parser.error(f"Selected calibration requires at least {required} values")

    period = 1.0 / args.rate_hz

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((args.host, args.port))
        server.listen(1)
        print(f"Mock stream listening on {args.host}:{args.port} at {args.rate_hz:.1f} Hz")

        while True:
            client, address = server.accept()
            print(f"Client connected from {address}")
            with client:
                client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                sample_index = 0
                deadline = time.monotonic()
                try:
                    while True:
                        client.sendall(make_packet(sample_index, args.active_areas, args.rate_hz))
                        sample_index += 1
                        deadline += period
                        now = time.monotonic()
                        if deadline < now - period:
                            deadline = now
                        time.sleep(max(0.0, deadline - now))
                except (ConnectionResetError, BrokenPipeError):
                    print("Client disconnected")


if __name__ == "__main__":
    main()
