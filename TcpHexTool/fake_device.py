#!/usr/bin/env python3
"""Sahte TCP cihazi - TcpHexTool'u fiziksel sistem yokken test etmek icin.

Bagli istemciye periyodik olarak veri cercevesi yollar ve gelen her
cerceveye ACK doner. Sadece Python standart kutuphanesi kullanir.

Kullanim:
    python3 fake_device.py                       # 0.0.0.0:5000, 1 sn'de bir veri
    python3 fake_device.py --port 5000 --interval 0.5
    python3 fake_device.py --no-periodic         # sadece gelene cevap ver
    python3 fake_device.py --ack "AA 55 81 00 FF"
    python3 fake_device.py --icd                 # gercek protokol: Cihazi Baslat
"""

import argparse
import select
import socket
import struct
import time


def hexs(data):
    """b'\\xaa\\x55' -> 'AA 55'"""
    return " ".join("%02X" % b for b in data)


def make_frame(counter):
    """Sahte sensor cercevesi: AA 55 <sayac> <deger_hi> <deger_lo> FF

    Deger, gercek bir olcum gibi yavasca salinsin diye sayaca bagli
    basit bir ucgen dalga uretiyor (0..1000 arasi).
    """
    value = abs((counter * 37) % 2000 - 1000)
    return b"\xAA\x55" + struct.pack(">BH", counter & 0xFF, value) + b"\xFF"


# Gercek protokol (Interface Control Document)
# Cihazi Baslat -> alt sistemdeki toplam modul sayisi
START_CMD = bytes.fromhex("000C0100C1B7")
START_RSP = bytes.fromhex("000C020005C147")


def serve_client(conn, addr, interval, ack, periodic, icd):
    print("[+] Baglandi: %s:%d" % addr)
    counter = 0
    next_tick = time.time() + interval

    while True:
        timeout = max(0.0, next_tick - time.time()) if periodic else None
        readable, _, _ = select.select([conn], [], [], timeout)

        if readable:
            data = conn.recv(4096)
            if not data:
                print("[-] Baglanti kapandi: %s:%d" % addr)
                return
            print("    RX: %s" % hexs(data))
            if icd:
                if data == START_CMD:
                    conn.sendall(START_RSP)
                    print("    TX: %s  (Cihazi Baslat cevabi)" % hexs(START_RSP))
                else:
                    print("    (bilinmeyen komut, cevap yok)")
            elif ack:
                conn.sendall(ack)
                print("    TX: %s  (ACK)" % hexs(ack))

        if periodic and time.time() >= next_tick:
            frame = make_frame(counter)
            conn.sendall(frame)
            print("    TX: %s" % hexs(frame))
            counter += 1
            next_tick += interval


def main():
    p = argparse.ArgumentParser(description="TcpHexTool icin sahte cihaz")
    p.add_argument("--host", default="0.0.0.0", help="dinlenecek adres")
    p.add_argument("--port", type=int, default=5000, help="dinlenecek port")
    p.add_argument("--interval", type=float, default=1.0,
                   help="periyodik veri araligi (saniye)")
    p.add_argument("--ack", default="AA 55 81 00 FF",
                   help="gelen veriye donulecek HEX cevap ('' ise cevap yok)")
    p.add_argument("--no-periodic", action="store_true",
                   help="kendiliginden veri gonderme, sadece cevap ver")
    p.add_argument("--icd", action="store_true",
                   help="gercek protokole gore davran: periyodik veri yok, "
                        "Cihazi Baslat komutuna gercek cevabi don")
    args = p.parse_args()

    ack = bytes.fromhex(args.ack.replace(" ", "")) if args.ack else b""

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.host, args.port))
    server.listen(1)
    print("[*] Dinleniyor: %s:%d  (Ctrl-C ile cik)" % (args.host, args.port))

    try:
        while True:
            conn, addr = server.accept()
            try:
                serve_client(conn, addr, args.interval, ack,
                             not args.no_periodic and not args.icd,
                             args.icd)
            except ConnectionResetError:
                print("[-] Baglanti koptu: %s:%d" % addr)
            finally:
                conn.close()
    except KeyboardInterrupt:
        print("\n[*] Kapatiliyor")
    finally:
        server.close()


if __name__ == "__main__":
    main()
