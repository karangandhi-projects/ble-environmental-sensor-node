# Vision

## Product Vision

Build a small but professionally structured BLE embedded product: a battery-conscious environmental sensor node that can be discovered, configured, monitored, and controlled from a phone or BLE central.

This is not a toy demo. It is a learning project designed to develop real engineering intuition around BLE systems.

## Why This Project Exists

BLE is common in embedded products because it solves a real problem: a low-power device needs to communicate with nearby phones, gateways, tools, or other embedded systems without consuming Wi-Fi-level power.

The project teaches:
- How BLE devices are discovered.
- How BLE data is modeled through GATT.
- How event-driven firmware interacts with a wireless stack.
- How notifications differ from reads/writes.
- How power and latency are traded off.
- How security and bonding fit into real products.
- How to debug interoperability issues.

## Target User

The target user is an embedded engineer who wants to add BLE as a real skill and understand it from product architecture down to firmware implementation.

## Product Story

A small sensor node wakes up, advertises itself, allows a phone to connect, publishes periodic environmental telemetry, accepts control commands, and stores configuration across reboot.

The project starts with simulated telemetry so BLE can be learned independently of hardware bring-up. Later, a real BME280/BMP280 sensor can replace the simulator.

## Success Criteria

A successful project should allow a developer to say:

> I designed and implemented a BLE peripheral with a custom GATT profile, notifications, writable control/configuration characteristics, persistent settings, and a documented firmware architecture.

## Long-Term Extensions

The same architecture can evolve into:
- Smart home sensor.
- BLE data logger.
- BLE provisioning companion.
- Wearable sensor node.
- BLE medical-device prototype.
- BLE OTA/DFU learning platform.
