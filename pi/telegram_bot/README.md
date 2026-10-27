# Telegram bot

Telegram bot for the Edge-Deception Gateway. It warns an IoT security engineer about suspected attacks and lets them choose what to do: ignore the attack or deploy the honeypot firmware.

## Files

- `bot.py`: the bot itself. It answers commands and handles the two buttons.
- `SendAlert.py`: test script that sends a sample alert with the buttons, so you can try the bot without the detection script.
- `config.example.py`: template for the bot token, chat ID, and OTA version file path

## How it works

1. An alert arrives in Telegram with two buttons. Alerts come from `DetectionScript.py`, or from `/emergency`, or from `SendAlert.py` for testing.
2. The engineer taps a button, and `bot.py` writes a version number into the OTA version file (`CodeVersion.txt`):

| Button | Version written | Effect |
|---|---|---|
| `DeployHoneyPot` | `1.1` | ESP32 boards running 1.0 see a newer version and update to the honeypot firmware |
| `IgnoreAttack` | `1.0` | Boards running the honeypot (1.1) see an older version and roll back to the normal firmware |

The ESP32 boards check this file through the OTA server, so the change reaches them on their next check.

## Commands

- `/start`: shows what the bot is for
- `/emergency`: sends the alert message with the two buttons

## Setup

1. Create a bot with **@BotFather** on Telegram and copy its token.
2. Copy `config.example.py` to `config.py` and fill in:
   - `API_TOKEN`: your bot token
   - `CHAT_ID`: the chat that receives alerts
   - `OTA_VERSION_FILE`: full path to `CodeVersion.txt` in the folder the OTA server serves

   Never commit `config.py` or `.env`.
3. Install the packages:

       pip install -r requirements.txt

4. Start the bot and keep it running:

       python bot.py

5. Optional test: send a sample alert.

       python SendAlert.py

`bot.py` must be running for the buttons to work. The detection script only sends the alert messages. This bot is what reacts when you tap a button.
