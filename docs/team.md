# Team

A four-person project. Each member built one part of the system.

| Person | Built | Main files |
|---|---|---|
| **Anwar** | ESP32 sensors and MQTT telemetry | `esp32/meter_node/` and Business |
| **Doaa** | Raspberry Pi setup, Mosquitto broker (TLS, users, ACL), FireBase dashboard | `pi/broker/`, `pi/mqtt_logger` |
| **Haneen** | OTA update, local OTA server, Telegram bot, honeypot firmware | `esp32/honeypot/`, `pi/ota_server/`, `pi/telegram_bot/` |
| **Mona** | AI detection (rate check and Isolation Forest), Node-RED | `pi/detection` |

## How the parts connect

- **Anwar → Mona:** the ESP32 publishes telemetry that the detector checks (fields and topics are in [interfaces.md](interfaces.md)).
- **Mona → Haneen:** when the detector finds an attack, the Telegram bot sends the alert with a Deploy Honeypot button.
- **Haneen → Anwar:** the button changes the version in `ota_server/CodeVersion.txt`, and the ESP32 downloads the honeypot firmware.
- **Doaa:** runs the broker and the Pi that everything else depends on.

## Working agreements

- One branch per person, merged through pull requests.
- No passwords, tokens, or keys in the repo (see `.gitignore`, and `secrets.h.example`).
