# Edge-Deception-Gateway

Edge-powered IIoT security gateway: detects DDoS on ESP32 smart meters using a Raspberry Pi, then switches the node to a honeypot via OTA, with Telegram alerts.

```mermaid

flowchart LR
  R["Edge-Deception-Gateway"] --> D["docs"]
  R --> E["esp32"]
  R --> P["pi"]
  R --> N["node-red"]
  R --> F["firebase"]

  D --> D1["architecture.md"]
  D --> D2["interfaces.md"]
  D --> D3["pi-setup.md"]

  E --> E1["honey_pot"]
  E --> E2["meter_node"]
  E1 --> E1a["honey_pot.ino"]
  E1 --> E1b["secrets.h.example"]
  E2 --> E2a["meter_node.ino"]
  E2 --> E2b["secrets.h.example"]

  P --> P1["broker"]
  P --> P2["detection"]
  P --> P3["mqtt_logger"]
  P --> P4["ota_server"]
  P --> P5["telegram_bot"]

  P1 --> P1a["mosquitto"]
  P1 --> P1b["docker-compose.yml"]
  P1 --> P1c["generate_certs.sh"]

  P1a --> P1a1["acl.conf"]
  P1a --> P1a2["mosquitto.conf"]

  P2 --> P2a["DetectionScript.py"]

  P3 --> P3a["MQTTtojson.py"]
  P3 --> P3b["edgeguard_sample.jsonl"]

  P4 --> P4a["CodeVersion.txt"]

  P5 --> P5a["SendAlert.py"]
  P5 --> P5b["bot.py"]
  P5 --> P5c["config.py"]

  N --> N1["attacker"]
  F --> F1["dashboard"]

```
