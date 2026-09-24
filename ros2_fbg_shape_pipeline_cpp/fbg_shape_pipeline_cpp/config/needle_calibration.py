"""Sensor calibration, normalized to millimeters for real and simulated input."""
import math
import re


def load_needle_config(path):
    aliases = {
        "needle_length_mm": ("needlelengthmm", "needlelength", "needletotallengthmm", "needletotallength", "totallengthmm"),
        "sensor_arc_lengths_mm": ("sensorarclengthsmm", "sensorarclengths", "arclengths"),
        "first_fbg_index": ("firstfbgindex", "firstfbg", "firstincludedfbgindex"),
        "curvature_scale": ("curvaturescale", "curvaturescales"),
        "orientation_sign": ("orientationsign", "orientationsigns"),
        "orientation_offset_rad": ("orientationoffsetrad", "orientationoffset"),
    }
    lookup = {alias: (key, 1.0) for key, names in aliases.items() for alias in names}
    for alias in ("needlelengthm", "needletotallengthm", "totallengthm"):
        lookup[alias] = ("needle_length_mm", 1000.0)
    lookup["sensorarclengthsm"] = ("sensor_arc_lengths_mm", 1000.0)
    result = {}
    with open(path, encoding="utf-8-sig") as handle:
        for raw in handle:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            parts = re.split("[:=]", line, maxsplit=1)
            if len(parts) != 2:
                raise ValueError(f"Invalid calibration line: {line}")
            key = re.sub("[^a-z0-9]", "", parts[0].lower())
            if key not in lookup:
                raise ValueError(f"Unknown calibration key: {parts[0]}")
            canonical, factor = lookup[key]
            if canonical in result:
                raise ValueError(f"Duplicate calibration key: {canonical}")
            tokens = parts[1].replace("[", " ").replace("]", " ").replace(",", " ").split()
            values = [float(token) * factor for token in tokens]
            if not values or not all(math.isfinite(v) for v in values):
                raise ValueError(f"Expected finite values for {canonical}")
            if canonical in ("needle_length_mm", "first_fbg_index"):
                if len(values) != 1:
                    raise ValueError(f"Expected one value for {canonical}")
                result[canonical] = values[0]
            else:
                result[canonical] = values
    missing = set(aliases) - {"curvature_scale"} - set(result)
    if missing:
        raise ValueError(f"Missing calibration keys: {sorted(missing)}")
    first = result["first_fbg_index"]
    if first < 1 or first != int(first):
        raise ValueError("First FBG must be a positive integer")
    result["first_fbg_index"] = int(first)
    positions = result["sensor_arc_lengths_mm"]
    if positions[0] < 0 or any(b <= a for a, b in zip(positions, positions[1:])):
        raise ValueError("Sensor positions must be nonnegative and strictly increasing")
    if result["needle_length_mm"] <= 0 or result["needle_length_mm"] < positions[-1]:
        raise ValueError("Needle length must be positive and reach the last sensor")
    legacy_scale = result.pop("curvature_scale", None)
    if legacy_scale is not None and (
        len(legacy_scale) != len(positions) or any(value != 1.0 for value in legacy_scale)
    ):
        raise ValueError("Legacy curvature scales are accepted only when all values equal 1")
    for key in ("orientation_sign", "orientation_offset_rad"):
        if len(result[key]) != len(positions):
            raise ValueError(f"{key} length must match sensor positions")
    if any(v not in (-1.0, 1.0) for v in result["orientation_sign"]):
        raise ValueError("Orientation signs must be +1 or -1")
    return result
