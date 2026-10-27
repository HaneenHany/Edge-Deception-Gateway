# ESP32 firmware

Firmware for the ESP32 smart meter node.

| Folder | Description |
|---|---|
| `meter_node/` | Main firmware: reads sensors, publishes telemetry over MQTT, and checks for OTA updates |
| `honeypot/` | Decoy firmware: serves a fake admin login page and reports every visit over MQTT |

## meter_node files

- `meter_node.ino`: collects sensor data from the ESP32 and handles OTA updates
- `secrets.example.h`: template for Wi-Fi, MQTT, and OTA settings

## honeypot files

- `honeypot.ino`: runs a decoy web server on port 80, publishes an alert (device ID, visitor IP, HTTP method, URI) to `edgeguard/v1/honeypot/<device_id>` over MQTT with TLS, and checks the OTA server for a firmware rollback
- `secrets.example.h`: template for Wi-Fi, MQTT, and OTA settings

## Setup

1. In the folder you are building, copy `secrets.example.h` to `secrets.h` and fill in your real values. Never commit `secrets.h`.
2. Open `meter_node/meter_node.ino` or `honeypot/honeypot.ino` in the Arduino IDE.
3. For the honeypot, install the **PubSubClient** library by Nick O'Leary from the Library Manager.
4. Upload once by USB cable. After that, updates arrive over the air.

## Versions

- Normal firmware (`meter_node`): `LOCAL_VERSION = "1.0"`
- Honeypot firmware (`honeypot`): `LOCAL_VERSION = "1.1"`

See `docs/interfaces.md` for topics, ports, and JSON fields.
