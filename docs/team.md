# Team

Who does what in the Edge-Deception Gateway project, the skills each part needs, and how to know it is done.

| Person | Owns |
|---|---|
| **Anwar** | ESP32 sensors and MQTT telemetry |
| **Doaa** | Raspberry Pi, Mosquitto, and the local OTA server |
| **Haneen** | OTA update and the Telegram bot |
| **Alaa** | AI detection and the Node-RED dashboard |

## Anwar: ESP32 sensors and MQTT telemetry

**Tasks**

1. Wire the voltage, current, and temperature sensors to the ESP32.
2. Read the sensor values and calibrate them.
3. Connect to Wi-Fi and to the Mosquitto broker (with username/password).
4. Build the JSON payload (`voltage`, `current`, `power`, `temp`) and publish it to `meter3/raw` every 2 seconds.
5. Reconnect automatically when Wi-Fi or MQTT drops.
6. Keep telemetry code in `telemetry.h` so it merges cleanly with the OTA code.

**Skills needed**

- C++ with the Arduino IDE
- ESP32 basics (Wi-Fi, ADC pins)
- Analog sensors and basic electronics (take care around mains voltage)
- MQTT and the `PubSubClient` library
- JSON building (`ArduinoJson`)

**Done when**

- `mosquitto_sub -t meter3/raw` shows a valid JSON message every 2 seconds.
- The ESP32 reconnects by itself after Wi-Fi is switched off and on.

**Learning links:** _add links here_

## Doaa: Raspberry Pi, Mosquitto, and local server

If the Pi is not available yet, build and test everything on a laptop first (Mosquitto and `http.server` run on Windows, Mac, and Linux), then repeat the setup on the Pi.

**Tasks**

1. Install Mosquitto and set up the username/password. Add an ACL: the ESP32 writes only to `raw`, and the dashboard reads only `clean` and `status`.
2. Set connection and rate limits on the broker.
3. Set up the Pi: OS, SSH, and a static IP.
4. Create `/home/pi/ota_server` and run `python3 -m http.server` from it as a systemd service, so it starts on boot and restarts if it crashes.
5. Open ports 1883 (MQTT) and the OTA port if a firewall is active.
6. Give Haneen write permission on the OTA folder.
7. Decide how Alaa's script may run the `iptables` command (a limited sudo rule).
8. Write the setup steps in `docs/pi-setup.md`.

**Skills needed**

- Linux command line
- Networking basics (IP addresses, ports, static IP, router settings)
- Mosquitto configuration (users, ACLs, limits)
- systemd services
- File permissions and `sudo`
- Basic firewall use (`ufw` or `iptables`)

**Done when**

- A message sent with `mosquitto_pub` from another machine reaches `mosquitto_sub`, and a wrong password is rejected.
- Opening `http://<pi-ip>:<port>` from another device shows `latest.txt` and `firmware.bin`.
- After a reboot, both services start by themselves.

**Learning links:** _add links here_

## Haneen: OTA update and Telegram bot

**Tasks**

1. Adapt the DroneBot server-pull sketch into `ota.h`: remove the OLED and button code, check for updates every 30 seconds, and set `SERVER_HOST` and `SERVER_PORT`.
2. Test the OTA by changing `latest.txt` by hand and watching the ESP32 update.
3. Build the honeypot firmware with `LOCAL_VERSION = "2.0"` that sends fake telemetry, so it does not update itself in a loop.
4. Create the Telegram bot with BotFather and keep the token private.
5. Write the bot script: alert message with Deploy Honeypot and Ignore buttons, a chat ID check that ignores anyone else, and a Deploy handler that writes `2.0` to `latest.txt`.
6. Subscribe to `meter3/alert` so the bot sends Alaa's alerts to Telegram.
7. Merge the OTA code into Anwar's sketch.

**Skills needed**

- Arduino C++ (`HTTPUpdate`, `WiFiClient`)
- How ESP32 OTA partitions work
- Python 3
- `python-telegram-bot` (async, inline buttons, callback handlers)
- `paho-mqtt`
- Basic file handling in Python

**Done when**

- Changing `latest.txt` from `1.0` to `2.0` makes the ESP32 update, reboot into the honeypot, and stay on `2.0`.
- Pressing Deploy in Telegram changes `latest.txt`, and a press from a different chat ID does nothing.

**Learning links:** _add links here_

## Alaa: AI detection and Node-RED

**Tasks**

1. Write the detection script that subscribes to `meter3/raw`.
2. Add a sliding-window message-rate counter to catch flooding.
3. Train an Isolation Forest on normal readings (real data from Anwar, or simulated) to catch odd values.
4. Republish normal messages to `meter3/clean`, and publish `GREEN` or `RED` on `meter3/status`.
5. On an attack, publish a JSON alert to `meter3/alert` and block the attacker's IP with `iptables` (agreed with Doaa).
6. Build the Node-RED dashboard: `mqtt in`, `json`, gauges and charts for `clean`, and a green/red indicator from `status`.
7. Test with fake messages and a simulated flood.

**Skills needed**

- Python 3 and `paho-mqtt`
- Basic machine learning with `scikit-learn` (anomaly detection, choosing features)
- Node-RED flows and the dashboard nodes
- MQTT topics
- Basic Linux and `iptables`

**Done when**

- Normal fake messages appear on `clean` and the dashboard stays green.
- A burst of fake messages turns the dashboard red, publishes an alert, and no further messages reach `clean`.

**Learning links:** _add links here_

## Handoffs between people

- **Anwar and Haneen** edit the same ESP32 sketch. Keep telemetry code in `telemetry.h` and OTA code in `ota.h`.
- **Anwar and Alaa** agree on topics and JSON fields (see `interfaces.md`).
- **Alaa and Haneen:** Alaa's detector publishes to `meter3/alert`, and Haneen's bot sends it to Telegram.
- **Alaa and Doaa:** blocking the attacker's IP needs `sudo` on the Pi, so Doaa decides how Alaa's script may run it.
- **Doaa and Haneen:** Doaa runs the OTA server, and Haneen puts `latest.txt` and `firmware.bin` in its folder.

## Workflow

1. Work on your own branch (for example `anwar/telemetry`).
2. Open a pull request, and have a teammate review it.
3. Never push to `main` directly.
4. Never commit passwords or tokens.
