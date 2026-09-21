"""Linux smoke test of TCP -> ROS -> external OpenIGTLink wire output."""
import math
import os
from pathlib import Path
import signal
import socket
import struct
import subprocess
import tempfile
import threading
import time
import unittest

from test_contracts import simulator

try:
    import rclpy
    from geometry_msgs.msg import PoseArray
    from rclpy.node import Node
    HAS_ROS = True
except ImportError:
    HAS_ROS = False


class Source:
    def __init__(self, straight=False):
        self.listener = socket.socket()
        self.listener.bind(("127.0.0.1", 0))
        self.port = self.listener.getsockname()[1]
        self.listener.listen(1)
        self.listener.settimeout(0.1)
        self.stop = threading.Event()
        self.error = False
        self.straight = straight
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        while not self.stop.is_set():
            try:
                client, _ = self.listener.accept()
            except socket.timeout:
                continue
            with client:
                client.settimeout(0.2)
                client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                index = 0
                while not self.stop.is_set():
                    packet = bytearray(simulator.make_packet(index, 20))
                    offset = 5
                    while offset < len(packet):
                        size, field_id = struct.unpack_from("<IH", packet, offset)
                        if field_id == 0:
                            struct.pack_into("<H", packet, offset + 6, int(self.error))
                        if self.straight and field_id in (3, 4):
                            count = struct.unpack_from("<I", packet, offset + 6)[0]
                            struct.pack_into(f"<{count}f", packet, offset + 10, *([0.0]*count))
                        offset += 4 + size
                    try:
                        client.sendall(packet)
                    except OSError:
                        break
                    index += 1
                    self.stop.wait(0.01)

    def close(self):
        self.stop.set()
        self.thread.join(timeout=2)
        self.listener.close()


@unittest.skipUnless(HAS_ROS and os.name == "posix", "requires Linux ROS 2 workspace")
class PipelineTests(unittest.TestCase):
    def test_wire_output_live_switch_and_error_pause(self):
        sources = [Source(), Source(straight=True)]
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            igtl_port = reservation.getsockname()[1]
        rclpy.init()
        node = Node("audit_pipeline_observer")
        shapes = []
        node.create_subscription(PoseArray, "/needle/state/current_shape", lambda msg: shapes.append(msg), 1)
        wire = []
        wire_stop = threading.Event()
        connection = None
        reader = None
        log = tempfile.TemporaryFile(mode="w+")
        process = subprocess.Popen(
            ["ros2", "launch", "ros2_smartneedle_adapter", "full_pipeline.launch.py",
             f"tcp_port:={sources[0].port}", f"bridge_port:={igtl_port}"],
            stdout=log, stderr=subprocess.STDOUT, start_new_session=True,
        )

        def pump(seconds):
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                rclpy.spin_once(node, timeout_sec=0.01)

        try:
            deadline = time.monotonic() + 20
            while not shapes and time.monotonic() < deadline:
                pump(0.1)
            self.assertTrue(shapes, "No reconstructed shapes")
            self.assertEqual(len(shapes[-1].poses), 19)
            connection = socket.create_connection(("127.0.0.1", igtl_port), timeout=5)
            connection.settimeout(0.1)

            def exact(count):
                data = b""
                while len(data) < count and not wire_stop.is_set():
                    try:
                        chunk = connection.recv(count-len(data))
                    except socket.timeout:
                        continue
                    if not chunk:
                        return None
                    data += chunk
                return data if len(data) == count else None

            def read_wire():
                while not wire_stop.is_set():
                    header = exact(58)
                    if header is None:
                        break
                    _, kind, name, _, length, _ = struct.unpack(">H12s20sQQQ", header)
                    body = exact(length)
                    if body is None:
                        break
                    wire.append((kind.rstrip(b"\0"), name.rstrip(b"\0"), body))

            reader = threading.Thread(target=read_wire, daemon=True)
            reader.start()
            pump(2)
            points = [item for item in wire if item[0] == b"POINT"]
            self.assertGreater(len(points), 80, "Transport failed to sustain even 40 Hz")
            self.assertEqual(points[-1][1], b"NeedleShape")
            self.assertEqual(len(points[-1][2]), 19*136)
            xyz = struct.unpack_from(">fff", points[-1][2], 18*136+100)
            self.assertTrue(all(math.isfinite(value) for value in xyz))

            result = subprocess.run(
                ["ros2", "param", "set", "/tcp_receiver_node", "tcp_port", str(sources[1].port)],
                capture_output=True, text=True, timeout=10,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            pump(1)
            tip = shapes[-1].poses[-1].position
            self.assertAlmostEqual(tip.z, 184.799378094435, places=6)
            self.assertAlmostEqual(tip.x, 0, places=6)
            self.assertAlmostEqual(tip.y, 0, places=6)
            self.assertIsNone(process.poll(), "Launch stopped during live switching")

            sources[1].error = True
            pump(1)
            count = len(wire)
            pump(0.6)
            self.assertEqual(len(wire), count, "Wire output continued after sensor errors/stale cutoff")
            sources[1].error = False
            pump(1)
            self.assertGreater(len(wire), count, "Output did not recover")
        finally:
            wire_stop.set()
            if reader:
                reader.join(timeout=2)
            if connection:
                connection.close()
            os.killpg(process.pid, signal.SIGINT)
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            for source in sources:
                source.close()
            node.destroy_node()
            rclpy.shutdown()
            log.seek(0)
            if process.returncode not in (0, -signal.SIGINT):
                print(log.read()[-6000:])
            log.close()
