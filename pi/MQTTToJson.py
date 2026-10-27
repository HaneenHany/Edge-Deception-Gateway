#!/usr/bin/env python3
"""
Subscribe to the Mosquitto broker over TLS and save every message
to a JSON Lines file (one JSON object per line).

Run:
    export MQTT_PASS='your-gateway-password'
    python3 mqtt_to_json.py

Install:
    pip install paho-mqtt
"""
import json
import os
import signal
import ssl
import sys
import time

import paho.mqtt.client as mqtt

# ---------------- Settings (override with environment variables) ----------------
HOST = os.getenv("MQTT_HOST", "localhost")
PORT = int(os.getenv("MQTT_PORT", "8883"))
USER = os.getenv("MQTT_USER", "gateway")
PASSWORD = os.getenv("MQTT_PASS", "")
CA_CERT = os.getenv(
    "MQTT_CA", os.path.expanduser("~/TLS_mosquitto/mosquitto/certs/ca.crt")
)
TOPIC = os.getenv("MQTT_TOPIC", "edgeguard/v1/#")
JSONL_PATH = os.getenv("JSONL_PATH", os.path.expanduser("~/TLS_mosquitto/edgeguard_data.jsonl"))
# Only for debugging a hostname mismatch with the certificate. Do not leave on.
INSECURE = os.getenv("MQTT_INSECURE", "0") == "1"
# --------------------------------------------------------------------------------

out = open(JSONL_PATH, "a", encoding="utf-8")


def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0 or getattr(reason_code, "is_failure", False) is False:
        print(f"Connected. Subscribing to {TOPIC}")
        client.subscribe(TOPIC, qos=1)
    else:
        print(f"Connection failed: {reason_code}")


def on_message(client, userdata, msg):
    raw = msg.payload.decode("utf-8", errors="replace")
    data = None
    device_id, ts = None, None
    try:
        data = json.loads(raw)
        if isinstance(data, dict):
            device_id = data.get("device_id")
            ts = data.get("ts")
    except json.JSONDecodeError:
        pass  # not JSON: saved as raw text under "data"

    record = {
        "received_at": time.time(),  # when the Pi received it
        "topic": msg.topic,
        "device_id": device_id,
        "ts": ts if ts is not None else time.time(),  # measurement time
        "data": data if data is not None else raw,
    }
    out.write(json.dumps(record, ensure_ascii=False) + "\n")
    out.flush()  # write to disk right away so nothing is lost on power cut
    print(f"saved  {msg.topic}  {raw[:80]}")


def on_disconnect(client, userdata, *args):
    print("Disconnected. paho will retry automatically.")


# paho-mqtt 2.x needs the callback API version; 1.x does not accept it.
try:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
except AttributeError:
    client = mqtt.Client()

client.username_pw_set(USER, PASSWORD)
client.tls_set(ca_certs=CA_CERT, tls_version=ssl.PROTOCOL_TLS_CLIENT)
if INSECURE:
    client.tls_insecure_set(True)

client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect
client.reconnect_delay_set(min_delay=1, max_delay=30)


def shutdown(*_):
    print("Stopping...")
    client.disconnect()
    out.close()
    sys.exit(0)


signal.signal(signal.SIGINT, shutdown)
signal.signal(signal.SIGTERM, shutdown)

client.connect(HOST, PORT, keepalive=60)
client.loop_forever()
