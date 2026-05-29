#!/usr/bin/env python3
"""
Run Unity on-target tests and collect results.

Prerequisites:
  - test_app must already be flashed: cd firmware/test_app && idf.py flash

Usage (from repo root):
  ! python3 firmware/test_app/run_tests.py
  ! python3 firmware/test_app/run_tests.py --port /dev/ttyACM0

Opening the port resets the ESP32-C3 (USB-CDC). The test_app runs all tests
automatically on boot (UNITY_BEGIN / unity_run_all_tests / UNITY_END) and
then drops into the interactive menu. This script captures the auto-run output.
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

PORT = "/dev/ttyACM0"
BAUD = 115200
BOOT_TIMEOUT = 30   # seconds — covers ESP32-C3 boot + full test run

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default=PORT)
    args = parser.parse_args()

    # Open WITHOUT asserting DTR/RTS — ESP32-C3 USB-CDC does not respond to
    # pyserial's DTR/RTS for reset, so we leave them alone and use the Unity
    # menu to trigger a run instead.
    s = serial.Serial()
    s.port = args.port
    s.baudrate = BAUD
    s.timeout = 0.5
    s.dtr = False
    s.rts = False
    s.open()
    s.reset_input_buffer()

    # Wake the Unity menu (harmless if already there) then run all tests.
    s.write(b"\r\n")
    time.sleep(0.3)
    s.write(b"*\r\n")

    print(f"Sent run-all to {args.port} — collecting results "
          f"(up to {BOOT_TIMEOUT}s) ...\n", flush=True)

    # Read until the Unity summary line or timeout.
    deadline = time.time() + BOOT_TIMEOUT
    passed = False
    summary_seen = False
    while time.time() < deadline:
        line = s.readline()
        if not line:
            continue
        decoded = line.decode("utf-8", errors="replace").rstrip()
        print(decoded, flush=True)
        # Unity prints "N Tests N Failures N Ignored" then "OK" or "FAIL"
        if b"Tests" in line and b"Failures" in line:
            summary_seen = True
        if summary_seen and (b"OK" in line or b"FAIL" in line):
            passed = b"OK" in line
            break

    s.close()

    if not summary_seen:
        print("\nTimeout — no test output received.")
        print("Make sure test_app is flashed (not main firmware).")
        sys.exit(1)
    elif not passed:
        print("\nFAIL — one or more tests failed.")
        sys.exit(1)
    else:
        print("\nAll tests passed.")


if __name__ == "__main__":
    main()
