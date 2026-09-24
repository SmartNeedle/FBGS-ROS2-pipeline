import importlib.util
import math
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "ros2_fbg_shape_pipeline_cpp/fbg_shape_pipeline_cpp/config"


def load(path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


calibration = load(CONFIG / "needle_calibration.py")
simulator = load(ROOT / "ros2_fbg_shape_pipeline_cpp/tools/mock_fbg_stream_server.py")


class ContractTests(unittest.TestCase):
    def test_current_sensor(self):
        config = calibration.load_needle_config(CONFIG / "needle_config.txt")
        self.assertEqual(config["first_fbg_index"], 3)
        self.assertEqual(len(config["sensor_arc_lengths_mm"]), 18)
        self.assertAlmostEqual(config["needle_length_mm"], 196.391633064447)
        self.assertEqual(config["orientation_sign"], [-1.0] * 18)
        self.assertNotIn("curvature_scale", config)

    def test_units_and_invalid_calibration(self):
        source = (CONFIG / "needle_config.txt").read_text()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sensor.txt"
            legacy = source.replace(
                "Orientation sign:",
                "Curvature scale: " + " ".join(["1"] * 18) + "\nOrientation sign:",
                1,
            )
            path.write_text(legacy)
            self.assertNotIn("curvature_scale", calibration.load_needle_config(path))
            path.write_text(source.replace("Needle length (mm): 196.391633064447", "Needle length (m): 0.196391633064447"))
            self.assertAlmostEqual(calibration.load_needle_config(path)["needle_length_mm"], 196.391633064447)
            for invalid in (
                source.replace("First FBG: 3", "First FBG: 2.5"),
                source.replace("Orientation sign:", "Curvature scale: " + " ".join(["1"] * 17 + ["2"]) + "\nOrientation sign:", 1),
                source.replace("11.592254970012", "nan", 1),
                source.replace("21.592254970012", "1", 1),
            ):
                path.write_text(invalid)
                with self.assertRaises(ValueError):
                    calibration.load_needle_config(path)

    def test_simulator_packet_and_curvature_bound(self):
        for index in range(300):
            packet = simulator.make_packet(index, 20)
            self.assertEqual(len(packet), 15767)
            self.assertEqual(struct.unpack_from("<I", packet)[0], len(packet) - 4)
            self.assertEqual(packet[4], 0)
            offset = 5
            fields = {}
            field_order = []
            while offset < len(packet):
                size, field_id = struct.unpack_from("<IH", packet, offset)
                end = offset + 4 + size
                self.assertLessEqual(end, len(packet))
                fields[field_id] = packet[offset + 6:end]
                field_order.append(field_id)
                offset = end
            self.assertEqual(offset, len(packet))
            self.assertEqual(set(fields), set(range(8)))
            self.assertEqual(field_order, [0, 1, 2, 4, 3, 5, 6, 7])
            self.assertEqual(struct.unpack_from("<II", fields[5]), (192, 3))
            spectra = fields[7]
            offset = 0
            channels = []
            while offset < len(spectra):
                block_length = struct.unpack_from("<I", spectra, offset)[0]
                end = offset + 4 + block_length
                self.assertLessEqual(end, len(spectra))
                channels.append(spectra[offset + 4])
                offset += 5
                counts = {}
                while offset < end:
                    sub_length, sub_id = struct.unpack_from("<IH", spectra, offset)
                    self.assertGreaterEqual(sub_length, 2)
                    if sub_id in (3, 4, 5, 6):
                        counts[sub_id] = struct.unpack_from("<I", spectra, offset + 6)[0]
                    offset += 4 + sub_length
                    self.assertLessEqual(offset, end)
                self.assertEqual(offset, end)
                self.assertEqual(counts, {3: 512, 4: 512, 5: 20, 6: 20})
            self.assertEqual(channels, [0, 1, 2, 3])
            for field_id in (3, 4, 6):
                self.assertEqual(struct.unpack_from("<I", fields[field_id])[0], 20)
                values = struct.unpack_from("<20f", fields[field_id], 4)
                self.assertTrue(all(math.isfinite(v) for v in values))
                if field_id == 3:
                    self.assertTrue(all(0 <= v <= 0.004 for v in values))
                if field_id == 4:
                    self.assertTrue(all(abs(v) <= math.pi for v in values))

    def test_motion_independent_of_rate(self):
        # Ignore timestamps and compare the curvature/angle fields at t=1 second.
        a = simulator.make_packet(100, 20, 100)
        b = simulator.make_packet(50, 20, 50)
        def sensor_fields(packet):
            offset = 5
            fields = {}
            while offset < len(packet):
                size, field_id = struct.unpack_from("<IH", packet, offset)
                if field_id in (3, 4):
                    fields[field_id] = packet[offset + 6:offset + 4 + size]
                offset += 4 + size
            return fields

        self.assertEqual(sensor_fields(a), sensor_fields(b))


if __name__ == "__main__":
    unittest.main()
