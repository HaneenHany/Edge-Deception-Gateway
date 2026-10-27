import json
import math
import os
import time
from collections import deque
import telebot
from telebot.types import InlineKeyboardButton, InlineKeyboardMarkup
import numpy as np
from sklearn.ensemble import IsolationForest
from sklearn.preprocessing import StandardScaler  # Added for feature scaling
import requests  # Added for Firebase REST API

from config import API_TOKEN, CHAT_ID, FIREBASE_URL, FB_AUTH   # secrets now live in config.py / .env

# ---------------- Settings ----------------
TOPIC = "edgeguard/v1/telemetry"
HISTORY_FILE = os.path.expanduser("~/TLS_mosquitto/edgeguard_data.jsonl")
# False: only process lines written after the script starts (recommended)
# True : process the whole file first, then keep following it
START_FROM_BEGINNING = False

# ---------------- Firebase Settings ----------------
# FIREBASE_URL and FB_AUTH are imported from config.py (see above)

WINDOW_SEC = 30          # time window for counting messages
RATE_TOLERANCE = 1.5     # allow 50% above the normal rate
ALERT_COOLDOWN = 60      # seconds between Telegram alerts of the same type
FB_ALERT_COOLDOWN = 10   # seconds between Firebase alerts of the same type (0 = every event)

# Allowed range for each sensor: (min, max)
RANGES = {
    "temperature": (0, 40),
    "humidity":    (10, 90),
    "voltage":     (0, 15),
    "current":     (0, 1),
    "power":       (0, 15),
}
FEATURES = list(RANGES)

# ---------------- Flags ----------------
Too_Many_Requests = False
READING_OUT_OF_RANGE = False

bot = telebot.TeleBot(token=API_TOKEN)

keyboard = InlineKeyboardMarkup(row_width=2)
keyboard.add(
    InlineKeyboardButton(text="DeployHoneyPot", callback_data="DeployHoneyPot"),
    InlineKeyboardButton(text="IgnoreAttack", callback_data="IgnoreAttack"),
)


# ---------------- Firebase push (from the working test) ----------------
def push_firebase(data, status, too_many, out_of_range):
    """Send a NORMAL reading to /readings and keep /latest updated."""
    record = {
        **(data if isinstance(data, dict) else {}),
        "status": status,
        "too_many_requests": bool(too_many),
        "out_of_range": bool(out_of_range),
        "ts": int(time.time() * 1000),
    }
    params = {"auth": FB_AUTH}
    base = FIREBASE_URL.rstrip("/")
    try:
        r1 = requests.post(f"{base}/readings.json", params=params, json=record, timeout=5)
        r2 = requests.put(f"{base}/latest.json", params=params, json=record, timeout=5)
        if not (r1.ok and r2.ok):
            print(f"Firebase rejected: readings={r1.status_code} {r1.text[:120]} | "
                  f"latest={r2.status_code} {r2.text[:120]}")
        return r1.ok and r2.ok
    except requests.RequestException as e:
        print("Firebase error:", e)
        return False


# ---------------- Firebase alert (NO sensor data inside) ----------------
last_fb_alert = {}


def push_firebase_alert(too_many, out_of_range):
    """Send ONLY an alert record to /alerts and /latest_alert (never the data itself)."""
    kinds = []
    if too_many:
        kinds.append("TOO_MANY_REQUESTS")
    if out_of_range:
        kinds.append("READING_OUT_OF_RANGE")
    kind = "+".join(kinds)

    now = time.time()
    if now - last_fb_alert.get(kind, 0) < FB_ALERT_COOLDOWN:
        return True                             # throttled, skip silently
    last_fb_alert[kind] = now

    record = {
        "alert": True,
        "type": kind,
        "too_many_requests": bool(too_many),
        "out_of_range": bool(out_of_range),
        "ts": int(now * 1000),
    }
    params = {"auth": FB_AUTH}
    base = FIREBASE_URL.rstrip("/")
    try:
        r1 = requests.post(f"{base}/alerts.json", params=params, json=record, timeout=5)
        r2 = requests.put(f"{base}/latest_alert.json", params=params, json=record, timeout=5)
        if not (r1.ok and r2.ok):
            print(f"Firebase alert rejected: alerts={r1.status_code} {r1.text[:120]} | "
                  f"latest_alert={r2.status_code} {r2.text[:120]}")
        return r1.ok and r2.ok
    except requests.RequestException as e:
        print("Firebase alert error:", e)
        return False


# ---------------- Learn from the saved file ----------------
def load_history(path):
    """Return (timestamps, sensor rows) of telemetry lines only."""
    times, rows = [], []
    with open(path) as f:
        for line in f:
            try:
                rec = json.loads(line)
                d = rec["data"]
                if rec["topic"] != TOPIC or not isinstance(d, dict):
                    continue            # skip online/offline status lines
                rows.append([d.get(k, 0) for k in FEATURES])
                times.append(rec["received_at"])
            except (ValueError, KeyError, TypeError):
                continue                # skip broken lines
    return np.array(times), np.array(rows)


times, rows = load_history(HISTORY_FILE)
if len(rows) < 2:
    raise SystemExit(
        f"Not enough telemetry in {HISTORY_FILE} to learn from "
        f"({len(rows)} rows). Let mqtt_to_json.py collect more data first."
    )

# Normal rate = typical (median) gap between telemetry messages
normal_interval = max(float(np.median(np.diff(times))), 0.01)   # avoid divide-by-zero
MAX_MSGS_PER_WINDOW = math.ceil(WINDOW_SEC / normal_interval * RATE_TOLERANCE)
print(f"Learned from {len(rows)} rows. Normal interval: {normal_interval:.2f}s "
      f"-> max {MAX_MSGS_PER_WINDOW} msgs per {WINDOW_SEC}s")

# Initialize and fit the StandardScaler to normalize feature ranges
scaler = StandardScaler()
scaled_rows = scaler.fit_transform(rows)

# Train Isolation Forest on the scaled historical rows
model = IsolationForest(contamination=0.05, random_state=42).fit(scaled_rows)

# ---------------- Checks ----------------
arrival_times = deque()


def check_rate(t):
    """t = time the message was received (taken from the file, not 'now')."""
    arrival_times.append(t)
    while t - arrival_times[0] > WINDOW_SEC:
        arrival_times.popleft()
    return len(arrival_times) > MAX_MSGS_PER_WINDOW


def check_range(data):
    # Only check keys that exist in the payload to handle missing elements gracefully
    return any(k in data and not (lo <= data[k] <= hi) for k, (lo, hi) in RANGES.items())


def check_isolation_forest(data):
    # Fallback if critical features are totally missing
    if not all(k in data for k in FEATURES):
        return True
    x = np.array([[data[k] for k in FEATURES]])
    x_scaled = scaler.transform(x)  # Scale live incoming data using the trained scaler
    return bool(model.predict(x_scaled)[0] == -1)


# ---------------- Telegram alerts ----------------
last_alert = {}


def alert(kind, text):
    """Send a Telegram message, at most once per ALERT_COOLDOWN per alert type."""
    now = time.time()
    if now - last_alert.get(kind, 0) < ALERT_COOLDOWN:
        return
    last_alert[kind] = now
    try:
        bot.send_message(CHAT_ID, text, reply_markup=keyboard)
    except Exception as e:
        print("Telegram error:", e)


# ---------------- Follow the file (like `tail -f`) ----------------
def follow(path, from_beginning=False):
    """Yield each new complete line appended to the file."""
    with open(path) as f:
        if not from_beginning:
            f.seek(0, os.SEEK_END)
        buf = ""
        while True:
            chunk = f.readline()
            if not chunk:
                time.sleep(0.5)
                if os.path.getsize(path) < f.tell():    # file was emptied/rotated
                    f.seek(0)
                    buf = ""
                continue
            buf += chunk
            if buf.endswith("\n"):                    # wait for a full line
                yield buf
                buf = ""


def handle(rec):
    global Too_Many_Requests, READING_OUT_OF_RANGE

    if not isinstance(rec, dict) or rec.get("topic") != TOPIC:
        return                                # skip status lines etc.

    data = rec.get("data")
    Too_Many_Requests = check_rate(rec.get("received_at", time.time()))

    try:
        if not isinstance(data, dict):
            raise TypeError("payload is not a JSON object")
        READING_OUT_OF_RANGE = check_range(data) or check_isolation_forest(data)
    except (ValueError, KeyError, TypeError):
        READING_OUT_OF_RANGE = True             # malformed or missing fields

    if Too_Many_Requests:
        alert("rate", "Potential hacking: too many requests in a short time! Choose your action:")
    if READING_OUT_OF_RANGE:
        alert("range", f"Reading out of range or abnormal: {data}\nChoose your action:")

    payload = json.dumps(data) if isinstance(data, dict) else str(data)

    if Too_Many_Requests or READING_OUT_OF_RANGE:
        # Attack / abnormal: send ONLY an alert to Firebase, never the data itself
        push_firebase_alert(Too_Many_Requests, READING_OUT_OF_RANGE)
        status = "BLOCKED"
    else:
        # Everything normal: send the reading to Firebase
        ok = push_firebase(data, "SENT", Too_Many_Requests, READING_OUT_OF_RANGE)
        status = "SENT" if ok else "ERROR"

    print(f"[{status}] Too_Many_Requests={Too_Many_Requests}  "
          f"READING_OUT_OF_RANGE={READING_OUT_OF_RANGE}  {payload}")


print(f"Following {HISTORY_FILE} ...")
for line in follow(HISTORY_FILE, START_FROM_BEGINNING):
    try:
        handle(json.loads(line))
    except Exception as e:
        print("skipped line:", e)
