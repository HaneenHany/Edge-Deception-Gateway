# OTA server files

Files served to the ESP32 for over-the-air updates.

- `CodeVersion.txt`: current firmware version only (`1.0` normal, `2.0` honeypot)
- `firmware.bin`: the firmware image (copied to the Pi, not stored in this repo)

  ## Run the OTA server

On the Pi, from the folder that contains `CodeVersion.txt` and the firmware `.bin` files:

    python3 -m http.server 8080
