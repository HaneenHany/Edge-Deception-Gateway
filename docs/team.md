# Team

Who does what in the Edge-Deception Gateway project, the skills each part needs, and how to know it is done.

| Person | Owns |
|---|---|
| **Anwar** | ESP32 sensors and MQTT telemetry |
| **Doaa** | Raspberry Pi, Mosquitto, the local OTA server, and the ThingsBoard dashboard |
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
7. Encrypt each JSON payload with AES-GCM before publishing to `meter3/raw`. Use a new nonce for every message and include a counter or timestamp. Keep the key in `secrets.h` only.

**Skills needed**

- C++ with the Arduino IDE
- ESP32 basics
- Analog sensors and basic electronics (take care around mains voltage)
- MQTT and the `PubSubClient` library
- JSON building (`ArduinoJson`)
- AES-GCM encryption on the ESP32 (`mbedtls`)

**Done when**

- `mosquitto_sub -t meter3/raw` shows a new message every 2 seconds.
- The ESP32 reconnects by itself after Wi-Fi is switched off and on.
- The messages on `meter3/raw` are unreadable (encrypted), and Alaa's script can decrypt them.

**Learning links:** _add links here_

## Doaa: Raspberry Pi, Mosquitto, local server, and ThingsBoard

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
9. Set up the ThingsBoard dashboard to show the live readings and the green/red status.

**Skills needed**

- Linux command line
- Networking basics (IP addresses, ports, static IP, router settings)
- Mosquitto configuration (users, ACLs, limits)
- systemd services
- File permissions and `sudo`
- Basic firewall use (`ufw` or `iptables`)
- ThingsBoard basics (devices, dashboards, widgets)

**Done when**

- A message sent with `mosquitto_pub` from another machine reaches `mosquitto_sub`, and a wrong password is rejected.
- Opening `http://<pi-ip>:<port>` from another device shows `latest.txt` and `firmware.bin`.
- After a reboot, both services start by themselves.
- The ThingsBoard dashboard shows live readings from the broker.

**Learning links:** _add links here_

## Haneen: OTA update and Telegram bot

**Tasks**

1. Adapt the DroneBot server-pull sketch into `ota.h`: remove the OLED and button code, check for updates every 30 seconds, and set `SERVER_HOST` and `SERVER_PORT`.
2. Test the OTA by changing `latest.txt` by hand and watching the ESP32 update.
3. Build the honeypot firmware with `LOCAL_VERSION = "2.0"` that sends
