# ESP32 firmware

Firmware for the ESP32 smart meter node.

| Folder | Description |
|---|---|
| `meter_node/` | Main firmware: reads sensors, publishes telemetry over MQTT, and checks for OTA updates |
| `honeypot/` | Decoy firmware (version 2.0): sends fake telemetry and logs unauthorized commands |

## meter_node files

- `meter_node.ino` collecting data from esp and OTA update
- `secrets.example.h`: template for Wi-Fi, MQTT, and OTA settings

## Setup

1. Copy `secrets.example.h` to `secrets.h` and fill in your real values. Never commit `secrets.h`.
2. Open `meter_node/meter_node.ino` in the Arduino IDE.
3. Upload it once by USB cable. After that, updates arrive over the air.

## Versions

- Normal firmware: `LOCAL_VERSION = "1.0"`
- Honeypot firmware: `LOCAL_VERSION = "2.0"`

See `docs/interfaces.md` for topics, ports, and JSON fields.
