# Interfaces

Everything one component needs to know about another: ports, MQTT topics, JSON fields, files, and versions.

## Network and ports

| Port | Protocol | Runs on | Purpose |
|---|---|---|---|
| 8883 | MQTT over TLS 1.2 | Raspberry Pi (Mosquitto in Docker, container `iiot-mosquitto`) | The only broker listener. Anonymous access is off. |
| 8080 | HTTP (plain, local network only) | OTA host | Serves `CodeVersion.txt` and the firmware `.bin` files. |
| 80 | HTTP | ESP32 in honeypot mode | Decoy "Smart Meter - Admin Login" page. |

## MQTT

Broker users (see `pi/mosquitto/config/acl.conf`): `gateway` (Pi scripts) and `esp32-lab-01` (the ESP32; MQTT client id `esp32-Device1`).

| Topic | Published by | Retained | Content |
|---|---|---|---|
| `edgeguard/v1/telemetry` | ESP32 meter | no | Sensor JSON every 5 s (below) |
| `edgeguard/v1/status/esp32-lab-01` | ESP32 | yes | `online` on connect; `offline` as the Last Will (QoS 1) |
| `edgeguard/v1/honeypot/esp32-lab-01` | ESP32 in honeypot mode | no | One JSON per hit on the decoy page (below) |
| `edgeguard/v1/events` | gateway | - | Write permission is granted in the ACL; no script in this repo publishes to it yet |

`MQTTToJson.py` subscribes to `edgeguard/v1/#` (QoS 1) as `gateway`. The broker only delivers topics that the ACL lets `gateway` read.

> **ACL note:** `acl.conf` currently lets `esp32-lab-01` write only `telemetry` and its own `status` topic, and lets `gateway` read only `telemetry` and `status/+`. The honeypot topic above is not granted yet, so those messages need two extra ACL lines before they reach the gateway:
> `user esp32-lab-01` → `topic write edgeguard/v1/honeypot/esp32-lab-01`, and `user gateway` → `topic read edgeguard/v1/honeypot/+`.

### Telemetry payload

```json
{"temperature": 26.6, "humidity": 47.6, "voltage": 14.12, "current": 0.0, "power": 0.0}
```

| Field | Unit | Decimals | Notes |
|---|---|---|---|
| `temperature` | °C | 1 | Left out if the DHT read fails |
| `humidity` | % | 1 | Left out if the DHT read fails |
| `voltage` | V | 2 | Sampled over a 200 ms window |
| `current` | A | 3 | |
| `power` | W | 2 | |

The payload has no `device_id`; the device is identified by the topic. Maximum size is 256 bytes on the ESP32, and the broker rejects messages over 8192 bytes (`message_size_limit`).

### Honeypot payload

```json
{"device_id": "esp32-lab-01", "ip": "192.168.x.x", "method": "GET", "uri": "/admin"}
```

The URI is cut to 80 characters and quotes and newlines are replaced, so attacker text can't break the JSON. At most one message every 500 ms.

## Telemetry log (`edgeguard_data.jsonl`)

`MQTTToJson.py` writes one JSON object per line. `DetectionScript.py` reads this file.

| Field | Meaning |
|---|---|
| `received_at` | Unix time when the Pi received the message (used for the rate check) |
| `topic` | MQTT topic |
| `device_id` | From the payload; `null` for current firmware |
| `ts` | Measurement time from the payload, or `received_at` if absent |
| `data` | The parsed JSON payload, or the raw text if it isn't JSON (for example `online`) |

A 500-line example is in `data/sample/edgeguard_sample.jsonl`.

## Detection rules (`DetectionScript.py`)

| Check | Rule |
|---|---|
| Flood (`Too_Many_Requests`) | More than `ceil(30 s / normal interval x 1.5)` telemetry messages in a 30 s window. The normal interval is the median gap in the history file. |
| Out of range | Any field outside: temperature 0-40, humidity 10-90, voltage 0-15, current 0-1, power 0-15 |
| Anomaly (AI) | `IsolationForest` (contamination 0.05, `random_state` 42) on the 5 fields, scaled with `StandardScaler` and trained on the history file at start-up |
| Malformed | A missing field or a non-JSON payload counts as abnormal |
| Alert cooldown | Telegram: 60 s per alert type. Firebase: 10 s per alert type. |

## Telegram bot (`main.py`)

| Command / button | Effect |
|---|---|
| `/start` | Welcome message |
| `/emergency` | Sends the DeployHoneyPot / IgnoreAttack buttons |
| Button `DeployHoneyPot` | Writes `1.1` to `ota_server/CodeVersion.txt` |
| Button `IgnoreAttack` | Writes `1.0` to `ota_server/CodeVersion.txt` |

`DetectionScript.py` sends the same buttons with each alert.

## Firebase Realtime Database (REST)

| Path | Written when | Fields |
|---|---|---|
| `/readings` (push) and `/latest` | Reading is normal | the 5 sensor fields, `status` (`SENT`), `too_many_requests`, `out_of_range`, `ts` (ms) |
| `/alerts` (push) and `/latest_alert` | Attack or anomaly | `alert`, `type` (`TOO_MANY_REQUESTS`, `READING_OUT_OF_RANGE` or both joined with `+`), `too_many_requests`, `out_of_range`, `ts` (ms). **No sensor data.** |

## OTA update (HTTP, port 8080)

| File | Used by | Meaning |
|---|---|---|
| `CodeVersion.txt` | both firmwares, polled every 30 s | Target version: `1.1` = run honeypot, `1.0` = normal meter |
| `firmwareV1_1.bin` | meter firmware (v1.0) | Downloaded when the server version is newer than the local one. This is the honeypot. |
| `firmwareV1_0.bin` | honeypot firmware (v1.1) | Downloaded when the server version is older than the local one. This restores the meter. |

## Versions

| Component | Version |
|---|---|
| Mosquitto image | `eclipse-mosquitto:2.0.18` |
| TLS | 1.2 (CA-verified; certificates from `generate_certs.sh`) |
| Meter firmware | `1.0` (`EdgeDeceptionEsp.ino`) |
| Honeypot firmware | `1.1` (`ESPHoneyPot.ino`) |
| ESP32 Arduino core | _fill in the version you build with_ |
| ESP32 libraries | PubSubClient, DHT sensor library; _fill in versions_ |
| Python and packages | see `pi/requirements.txt`; _fill in your Python version_ |
| Raspberry Pi model / OS | _fill in_ |
