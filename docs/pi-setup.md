# Raspberry Pi / gateway setup

Step-by-step setup of the broker, the Python scripts, the OTA server, and the ESP32. Ports, topics, and file formats are in [interfaces.md](interfaces.md).

## 0. What runs where

| Component                                                 | Runs on                            | Notes                                                                           |
| --------------------------------------------------------- | ---------------------------------- | ------------------------------------------------------------------------------- |
| Mosquitto broker                                          | Raspberry Pi                       | Docker, TLS on 8883                                                             |
| `MQTTToJson.py`, `DetectionScript.py`                     | Raspberry Pi                       |                                                                                 |
| bot.py` (Telegram bot) and the OTA web server (port 8080) | One machine, **the same for both** | The bot writes `CodeVersion.txt`, and the web server serves it to the ESP32.    |
| ESP32 meter                                               | Same Wi-Fi network                 | Its `SECRET_MQTT_HOST` is the Pi; its `SECRET_OTA_SERVER_HOST` is the OTA host. |

## 1. Prerequisites

- Docker and Docker Compose, Python 3, git, OpenSSL
- Arduino IDE  with ESP32 board support and the libraries PubSubClient and DHT sensor library
- A Telegram bot token (from @BotFather) and your chat ID
- A Firebase Realtime Database (URL and database secret)

## 2. Get the code

```bash
git clone https://github.com/HaneenHany/Edge-Deception-Gateway.git
cd Edge-Deception-Gateway/pi
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
```

## 3. Certificates and broker users

Use the Pi's LAN IP:

```bash
./generate_certs.sh 192.168.x.x
```

Then create the broker users `gateway` and `esp32-lab-01`. The exact commands are in [pi/mosquitto/README.md](../pi/mosquitto/README.md). If the Pi's IP changes later, re-run `generate_certs.sh` with the new IP and run `docker restart iiot-mosquitto`. The CA stays the same, so the ESP32 doesn't need reflashing.

## 4. Start the broker

```bash
docker compose up -d
docker logs iiot-mosquitto
```

## 5. Configure secrets

```bash
cp .env.example .env      # fill in the Telegram, Firebase and MQTT values
```

`config.py` loads `.env` automatically (needs `python-dotenv`, which is in `requirements.txt`). `MQTTToJson.py` does **not**, so load the variables into your shell first:

```bash
set -a; source .env; set +a
export MQTT_CA=$PWD/mosquitto/certs/ca.crt
```

## 6. Collect telemetry

```bash
python3 MQTTToJson.py
```

This writes `~/TLS_mosquitto/edgeguard_data.jsonl`, which is also the file `DetectionScript.py` reads. Leave the default path so both scripts agree.

## 7. Train and run the detector

`DetectionScript.py` learns the normal rate and value pattern from the history file at start-up, and needs at least 2 telemetry rows. To start quickly from the example data:

```bash
mkdir -p ~/TLS_mosquitto
cp ../data/sample/edgeguard_sample.jsonl ~/TLS_mosquitto/edgeguard_data.jsonl
python3 DetectionScript.py
```

For better results, let `MQTTToJson.py` run for a while on the real meter first.

## 8. Telegram bot and OTA server

On the OTA host (copy the repo there if it's a different machine), from `ota_server/`:

```bash
python3 -m http.server 8080       # one simple way to serve the folder on port 8080
```

In another terminal on that same machine:

```bash
cd pi && python3 bot.py          # set the same TELEGRAM_* values in its .env
```

Put `firmwareV1_0.bin` (meter) and `firmwareV1_1.bin` (honeypot) in `ota_server/`. They are not in the repo, because compiled binaries contain your Wi-Fi and MQTT passwords. Build them yourself in the Arduino IDE with **Sketch → Export Compiled Binary** from each sketch, using your own `secrets.h`.

## 9. Flash the ESP32

1. In `esp32/meter_node/` and `esp32/honeypot/`, copy `secrets.h.example` to `secrets.h` and fill it in (Wi-Fi, Pi IP, OTA host IP, MQTT password).
2. If you regenerated the CA, replace the certificate block in both `.ino` files with the new `ca.crt`.
3. Flash `meter_node` (v1.0) over USB. The honeypot (v1.1) arrives later over the air.

## 10. Check that it works

1. The Serial Monitor shows the ESP32 connected to Mosquitto (TLS) and publishing every 5 s.
2. `edgeguard_data.jsonl` grows, and `DetectionScript.py` prints `[SENT]` lines.
3. Cause an anomaly (for example, publish many messages quickly, or a value out of range). You should get a Telegram alert with the two buttons.
4. Press **DeployHoneyPot**. Within about 30 s the ESP32 downloads `firmwareV1_1.bin`, reboots, and serves the decoy page on port 80.
5. Press **IgnoreAttack** (or set `CodeVersion.txt` to `1.0`) to switch back.

## Troubleshooting

| Problem | Likely cause |
|---|---|
| ESP32 can't connect over TLS | The server certificate doesn't list the Pi's current IP. Re-run `generate_certs.sh <IP>` and restart the container. The ESP32 clock must also be synced (NTP) for certificate dates to validate. |
| `Connection refused` / not authorized | Wrong `MQTT_PASS`, or the user isn't in the `passwd` file or `acl.conf`. |
| `Missing required setting(s)` on start | `.env` is missing or not filled in. |
| OTA never starts | The ESP32 can't reach the OTA host on port 8080, `CodeVersion.txt` isn't served, or the version isn't newer than the one on the device. |
| Compile errors in the sketch | Your ESP32 core or library versions differ from the ones listed in `interfaces.md`. |
