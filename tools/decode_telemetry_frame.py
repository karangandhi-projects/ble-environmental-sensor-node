#!/usr/bin/env python3
"""Decode BLE_ENV_NODE telemetry frames.

Usage:
    python tools/decode_telemetry_frame.py 01010300a08601009209a814cd8b0100
"""

import argparse
import struct


def decode(hex_string: str) -> dict:
    raw = bytes.fromhex(hex_string.replace(" ", ""))
    if len(raw) != 16:
        raise ValueError(f"expected 16 bytes, got {len(raw)}")
    version, flags, sequence, uptime_ms, temp_x100, hum_x100, pressure_pa = struct.unpack("<BBHIhHI", raw)
    return {
        "version": version,
        "flags": flags,
        "sensor_valid": bool(flags & 0x01),
        "simulated_data": bool(flags & 0x02),
        "low_battery": bool(flags & 0x04),
        "sequence": sequence,
        "uptime_ms": uptime_ms,
        "temperature_c": temp_x100 / 100.0,
        "humidity_pct": hum_x100 / 100.0,
        "pressure_pa": pressure_pa,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("hex_frame")
    args = parser.parse_args()
    for key, value in decode(args.hex_frame).items():
        print(f"{key}: {value}")


if __name__ == "__main__":
    main()
