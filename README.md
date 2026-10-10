# Edge-Deception-Gateway
# Edge-powered IIoT security gateway: detects DDoS on ESP32 smart meters using a Raspberry Pi, then switches the node to a honeypot via OTA, with Telegram alerts.
```mermaid
flowchart LR
  R["Edge-Deception-Gateway"] --> D["docs"]
  R --> E["esp32"]
  R --> P["pi"]
  R --> N["node-red"]
  E --> E1["meter_node"]
  E --> E2["honeypot"]
  P --> P1["mosquitto (Doaa)"]
  P --> P2["systemd (Doaa)"]
  P --> P3["ota_server (Haneen)"]
  P --> P4["detection (Alaa)"]
  P --> P5["telegram_bot (Haneen)"]
  N --> N1["flows.json (Alaa)"]
```
