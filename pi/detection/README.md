# Detection script

`DetectionScript.py` watches the smart meter telemetry and flags suspicious behavior. It follows the JSONL file written by `mqtt_to_json.py` (like `tail -f`), checks every new reading, sends Telegram alerts when something looks wrong, and pushes data or alerts to Firebase.

## How it works

1. **Learn:** on startup it reads the saved telemetry history and learns two things:
   - the normal message rate (median gap between messages)
   - the normal sensor behavior, by training an Isolation Forest on scaled historical readings
2. **Check:** each new telemetry line goes through three checks:

| Check | What it catches | Rule |
|---|---|---|
| Rate | Too many requests (flooding) | More than `ceil(30 s / normal interval × 1.5)` messages in a 30 s window |
| Range | Impossible sensor values | Any value outside its allowed range (see below) |
| Isolation Forest | Unusual combinations of values | Model marks the reading as an outlier. A reading with missing sensor fields also counts as abnormal. |

3. **React:**
   - **Normal reading:** sent to Firebase (`/readings` and `/latest`) with status `SENT`.
   - **Attack or abnormal reading:** only an alert is sent to Firebase (`/alerts` and `/latest_alert`). The sensor data itself is never forwarded, and the status is `BLOCKED`. A Telegram message with **DeployHoneyPot** and **IgnoreAttack** buttons is also sent.

## Allowed sensor ranges

| Sensor | Min | Max |
|---|---|---|
| temperature | 0 | 40 |
| humidity | 10 | 90 |
| voltage | 0 | 15 |
| current | 0 | 1 |
| power | 0 | 15 |

Edit the `RANGES` dictionary in the script to change them.

## Settings

| Setting | Default | Meaning |
|---|---|---|
| `TOPIC` | `edgeguard/v1/telemetry` | Only lines from this MQTT topic are checked |
| `HISTORY_FILE` | `~/TLS_mosquitto/edgeguard_data.jsonl` | File to learn from and follow |
| `START_FROM_BEGINNING` | `False` | `False` processes only new lines; `True` processes the whole file first |
| `WINDOW_SEC` | `30` | Time window for counting messages |
| `RATE_TOLERANCE` | `1.5` | Allowed rate above normal (1.5 = 50% more) |
| `ALERT_COOLDOWN` | `60` | Seconds between Telegram alerts of the same type |
| `FB_ALERT_COOLDOWN` | `10` | Seconds between Firebase alerts of the same type (0 = every event) |

## Requirements

- Python 3
- A running `mqtt_to_json.py` that writes telemetry to `HISTORY_FILE`
- At least 2 telemetry rows in that file, otherwise the script exits and asks you to collect more data
- Python packages:

```bash
pip install pyTelegramBotAPI numpy scikit-learn requests
```

## Setup

1. Create `config.py` next to the script (or load the values from `.env`) with:

```python
API_TOKEN = "..."     # Telegram bot token
CHAT_ID = "..."       # Telegram chat that receives alerts
FIREBASE_URL = "..."  # Firebase Realtime Database URL
FB_AUTH = "..."       # Firebase auth secret/token
```

2. Never commit `config.py` or `.env`. Add both to `.gitignore`.
3. Let `mqtt_to_json.py` collect some normal data first, so the model learns from clean readings.
4. Run:

```bash
python DetectionScript.py
```

On start it prints what it learned, for example `Learned from 500 rows. Normal interval: 2.00s -> max 23 msgs per 30s`. Then it prints one line per reading with its status (`SENT`, `BLOCKED`, or `ERROR`).

## Firebase structure

| Path | Content |
|---|---|
| `/readings` | History of normal readings |
| `/latest` | Most recent normal reading |
| `/alerts` | History of alerts (no sensor data) |
| `/latest_alert` | Most recent alert |
