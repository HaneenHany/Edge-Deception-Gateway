# Mosquitto (MQTT broker)

Owner: Doaa

Broker settings for the Raspberry Pi: port, authentication, topic permissions (ACL), and connection limits.

- `config/mosquitto.conf`: broker configuration (TLS-only listener on 8883, no anonymous access)
- `config/acl.conf`: topic permissions per user

## Files you create locally (not in the repo)

These are in `.gitignore` because they contain private keys or password hashes.

1. **Certificates** (run from the `pi/` folder, use the Raspberry Pi IP):

       ./generate_certs.sh 192.168.x.x

   This creates `mosquitto/certs/` with `ca.crt`, `ca.key`, `server.crt`, `server.key`.
   Only `ca.crt` is shared (the ESP32 sketches embed it). Never share `ca.key` or `server.key`.

2. **Password file** (one user for the gateway, one per ESP32):

       docker run --rm -it -v "$PWD/mosquitto/config:/mosquitto/config" eclipse-mosquitto:2.0.18 \
         mosquitto_passwd -c /mosquitto/config/passwd gateway
       docker run --rm -it -v "$PWD/mosquitto/config:/mosquitto/config" eclipse-mosquitto:2.0.18 \
         mosquitto_passwd /mosquitto/config/passwd esp32-lab-01

3. **Start the broker:** `docker compose up -d`
