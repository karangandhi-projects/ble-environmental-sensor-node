#!/usr/bin/env python3
"""
Run Unity on-target tests without flashing.

Prerequisites:
  - test_app must already be flashed: cd firmware/test_app && idf.py flash
  - Device must be at the Unity menu (boots there automatically after flash)

Usage (from repo root):
  ! python3 firmware/test_app/run_tests.py
  ! python3 firmware/test_app/run_tests.py --port /dev/ttyUSB0

The script opens the port WITHOUT asserting DTR/RTS, so the device is NOT
reset. If the device is already at the Unity menu prompt it will respond
immediately. If the main firmware is flashed instead, the device will not
respond to the Unity menu protocol — flash test_app first.
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

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default=PORT)
    args = parser.parse_args()

    # Open without asserting DTR/RTS — prevents reset of the ESP32-C3.
    # The device stays at the Unity menu prompt rather than rebooting.
    s = serial.Serial()
    s.port = args.port
    s.baudrate = BAUD
    s.timeout = 0.3
    s.dtr = False
    s.rts = False
    s.open()

    s.reset_input_buffer()

    # Step 1: Send ENTER to get past "Press ENTER to see the list of tests."
    # If the device is already past the first prompt, this is a no-op.
    s.write(b"\r\n")
    time.sleep(0.3)

    # Step 2: Send * to run all tests
    s.write(b"*\r\n")

    print(f"Running all Unity tests on {args.port} ...\n", flush=True)

    # Step 3: Read output until summary line or timeout (15s max)
    deadline = time.time() + 15
    passed = False
    while time.time() < deadline:
        line = s.readline()
        if not line:
            continue
        decoded = line.decode("utf-8", errors="replace").rstrip()
        print(decoded, flush=True)
        # Unity summary line: "N Tests M Failures K Ignored"
        if b"Tests" in line and (b"OK" in line or b"FAIL" in line):
            passed = b"OK" in line
            break

    s.close()

    if not passed:
        print("\nFAIL — tests did not all pass (or timed out reading output).")
        print("If no output appeared, make sure test_app is flashed (not main firmware).")
        sys.exit(1)
    else:
        print("\nAll tests passed.")


if __name__ == "__main__":
    main()
