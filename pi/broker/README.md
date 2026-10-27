# Mosquitto broker (TLS)

MQTT broker for the Edge-Deception Gateway. It runs on the Raspberry Pi in Docker and accepts only TLS connections on port 8883. The ESP32 boards publish to it, and `MQTTToJson.py` subscribes to it.

## Files

- `docker-compose.yml`: runs Mosquitto 2.0.18 in a container named `iiot-mosquitto`
- `generate_certs.sh`: creates the CA and the broker's server certificate
- `mosquitto/config/mosquitto.conf`: broker settings (see below)

## What is not in the repo

These are created on the Pi and must never be committed:

- `mosquitto/certs/` (contains private keys)
- `mosquitto/config/passwd` (user password hashes)
- `mosquitto/data/` and `mosquitto/log/`

## Setup

Run everything from this folder, because the paths are relative.

### 1. Generate the certificates

Give the script the Pi's LAN IP address:

    bash generate_certs.sh 192.168.x.x

This creates `mosquitto/certs/` with:

| File | Share it? |
|---|---|
| `ca.crt` | Yes. This is the public CA certificate. |
| `ca.key` | No. Never. |
| `server.crt` | Not needed outside the Pi |
| `server.key` | No. Never. |

If the Pi's IP changes later, run the script again with the new IP. It reuses the existing CA, so `ca.crt` stays the same and the ESP32 boards do not need re-flashing. Then restart the broker:

    docker restart iiot-mosquitto

To delete everything and start from scratch, use `--reset`:

    bash generate_certs.sh --reset 192.168.x.x

A new CA means a new `ca.crt`. In that case, paste the new certificate into the ESP32 sketches and flash them again.

### 2. Create the broker config

`mosquitto/config/mosquitto.conf` must enable TLS on port 8883, point to the certificates, and require a password. It should contain at least:

    listener 8883
    cafile /mosquitto/certs/ca.crt
    certfile /mosquitto/certs/server.crt
    keyfile /mosquitto/certs/server.key
    allow_anonymous false
    password_file /mosquitto/config/passwd

### 3. Start the broker

    docker compose up -d

### 4. Create the MQTT users

The first command creates the password file. The second adds another user to it. Each command asks for a password.

    docker exec -it iiot-mosquitto mosquitto_passwd -c /mosquitto/config/passwd gateway
    docker exec -it iiot-mosquitto mosquitto_passwd /mosquitto/config/passwd esp32-lab-01

| User | Used by |
|---|---|
| `gateway` | `MQTTToJson.py` on the Pi (password goes in the `MQTT_PASS` environment variable) |
| `esp32-lab-01` | The ESP32 boards (password goes in `secrets.h`) |

Restart the broker so it picks up the users:

    docker restart iiot-mosquitto

## Useful commands

    docker compose up -d           # start
    docker compose down            # stop
    docker logs -f iiot-mosquitto  # watch the broker log
