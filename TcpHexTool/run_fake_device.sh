#!/bin/sh
# Gercek cihazin IP'sini loopback uzerinde taklit ederek sahte cihazi calistirir.
#
# Kullanim:
#   sudo ./run_fake_device.sh                       # 192.168.1.100:5000
#   sudo ./run_fake_device.sh 192.168.1.100 5000
#
# Ctrl-C ile ciktiginda eklenen IP otomatik silinir.
#
# DIKKAT: Bu IP aginda gercekten varsa cakisma olur.
# Sadece gercek cihaz bagli degilken kullan.

IP=${1:-192.168.1.100}
PORT=${2:-5000}

if [ "$(id -u)" != "0" ]; then
    echo "Bu betik root gerektiriyor: sudo $0 $IP $PORT"
    exit 1
fi

cleanup() {
    echo ""
    echo "[*] $IP loopback uzerinden siliniyor"
    ip addr del "$IP/32" dev lo 2>/dev/null
}
trap cleanup EXIT INT TERM

echo "[*] $IP loopback'e ekleniyor"
ip addr add "$IP/32" dev lo || exit 1

python3 "$(dirname "$0")/fake_device.py" --host "$IP" --port "$PORT"
