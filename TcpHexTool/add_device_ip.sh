#!/bin/sh
# Gercek cihazin IP'sini bu makinede gecici olarak olusturur.
#
# Simulatoru BASLATMAZ. Sadece adresi ekler, Ctrl-C'ye kadar bekler,
# cikarken siler. Simulatoru ayri bir terminalde kendi kullanicinla
# calistir ve arayuzdeki IP listesinden bu adresi sec.
#
# Kullanim:
#   sudo ./add_device_ip.sh                    # 192.168.1.100
#   sudo ./add_device_ip.sh 192.168.1.100
#   sudo ./add_device_ip.sh 192.168.1.100 192.168.1.120   # birden fazla
#
# DIKKAT: Bu IP aginda gercekten varsa cakisma olur.
# Sadece gercek cihaz bagli degilken kullan.

if [ "$(id -u)" != "0" ]; then
    echo "Bu betik root gerektiriyor: sudo $0 $*"
    exit 1
fi

IPS=${*:-192.168.1.100}

cleanup() {
    echo ""
    for ip in $IPS; do
        echo "[*] $ip siliniyor"
        ip addr del "$ip/32" dev lo 2>/dev/null
    done
}
trap cleanup EXIT INT TERM

for ip in $IPS; do
    echo "[*] $ip loopback'e ekleniyor"
    ip addr add "$ip/32" dev lo || exit 1
done

echo ""
echo "Adresler hazir. Simulatoru ayri bir terminalde calistir:"
echo "    python3 fake_device.py"
echo "ve arayuzdeki IP listesinden secim yap."
echo ""
echo "Ctrl-C ile adresleri sil ve cik."

# Ctrl-C gelene kadar bekle
while true; do
    sleep 3600
done
