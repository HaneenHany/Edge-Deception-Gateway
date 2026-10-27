#!/usr/bin/env bash
# Usage:  ./generate_certs.sh <IP> [more IPs...]      e.g. ./generate_certs.sh 192.168.1.50
#         ./generate_certs.sh --reset <IP>            (delete everything and start from scratch)
#
# First run : creates a CA (ca.crt / ca.key) + a server certificate for the given IP(s).
# Next runs : REUSES the existing CA and only re-issues the server certificate.
#             ca.crt stays the same, so the ESP32 does NOT need re-flashing when the IP changes.
# After re-issuing: docker restart iiot-mosquitto
set -e
RESET=0
if [ "$1" = "--reset" ]; then RESET=1; shift; fi
[ $# -ge 1 ] || { echo "Usage: ./generate_certs.sh [--reset] <IP> [more IPs...]"; exit 1; }

D="mosquitto/certs"; mkdir -p "$D"; cd "$D"
[ $RESET -eq 1 ] && rm -f ca.crt ca.key ca.srl server.crt server.key

if [ ! -f ca.key ] || [ ! -f ca.crt ]; then
  echo ">> Creating NEW CA"
  openssl genrsa -out ca.key 4096
  openssl req -new -x509 -days 3650 -key ca.key -out ca.crt -subj "/CN=IIoT-Local-CA"
else
  echo ">> Reusing existing CA (ca.crt unchanged)"
fi

# Each IP is added as IP: (Python/OpenSSL) and as DNS: (ESP32 mbedTLS matches the string)
SAN="DNS:localhost,IP:127.0.0.1"
for h in "$@"; do SAN="$SAN,IP:$h,DNS:$h"; done

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -subj "/CN=$1"
printf "subjectAltName=%s\n" "$SAN" > san.ext
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 825 -extfile san.ext
rm -f server.csr san.ext

echo "Server cert valid for: $SAN"
echo "Share ONLY ca.crt with the team. Never share ca.key / server.key."