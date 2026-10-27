# Attack simulator (Node-RED)

A Node-RED flow that plays the role of an attacker, so you can test the detection system without a real attack. It floods the same MQTT broker the ESP32 uses and sends its messages with the same device ID as the real board.

Use it only on your own lab network and your own broker.

## What it does

1. Connects to the Mosquitto broker with the ESP32's identity (same device ID and MQTT user).
2. Publishes a large number of telemetry messages in a short time to the telemetry topic (`edgeguard/v1/telemetry`).
3. Each message looks like a normal reading from the real board, so the traffic is hard to tell apart from the real device except by its rate.

## What it is for

It triggers the detection script (`DetectionScript.py`) on purpose, so you can check that the whole chain works:

| Step | What should happen |
|---|---|
| Broker | Receives the flood and the logger saves every message |
| `DetectionScript.py` | The rate check sees far more messages than normal in the 30 second window and sets `Too_Many_Requests` |
| Telegram | An alert arrives with the **DeployHoneyPot** and **IgnoreAttack** buttons |
| Firebase | Only an alert is stored. The flood data is not forwarded. |
| `DeployHoneyPot` | The OTA version file changes to `1.1`, and the ESP32 updates to the honeypot firmware |

## Files

- `flows.json`: the exported Node-RED flow

## Setup

1. Install Node-RED on the machine you will attack from (a laptop on the same network is fine).
2. Open Node-RED, then the menu, then **Import**, and select `flows.json`.
3. Open the MQTT broker settings in the flow and fill in:
   - the broker address (the Raspberry Pi's LAN IP) and port `8883`
   - TLS enabled, with the CA certificate (`ca.crt`) from the broker folder
   - the MQTT username and password of the ESP32 user
4. Click **Deploy**.
5. Click the inject button to start the flood.

## Before you run it

- The detection script and the Telegram bot must be running, or nothing will react.
- Turn the real ESP32 off, or expect two sources sending under one ID.
- Run it from a different machine than the Pi, to behave like a real attacker on the network.

