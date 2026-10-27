# systemd services

Service files that start the Pi's servers on boot and restart them if they crash.

- `ota-server.service`: runs the HTTP OTA file server

## ota-server.service

The OTA server is a simple web server that the ESP32 boards contact to check for firmware updates. It serves two things:

- `CodeVersion.txt`: the firmware version number the boards compare against their own
- the firmware `.bin` files the boards download when an update (or rollback) is needed

You can run it by hand with a normal Python command, from the folder that holds those files:

```bash
cd /path/to/ota_files
python3 -m http.server 8080
```

The service file does the same thing automatically, so you don't have to start it yourself after a reboot.

### Useful commands

```bash
sudo systemctl start ota-server      # start now
sudo systemctl stop ota-server       # stop
sudo systemctl enable ota-server     # start automatically on boot
sudo systemctl status ota-server     # check if it is running
journalctl -u ota-server -f          # watch the live log
```
