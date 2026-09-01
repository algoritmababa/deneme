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
#   Giden : 00 0C 01 00 | C1 B7
#   Gelen : 00 0C 02 00 05 | C1 47
#                       ^^ modul sayisi
#
# Komutu ilk 3 byte'tan tanitiyoruz; 4. byte'i entegrasyon kodu
# degistirebilir, o yuzden esitlik aramiyoruz.
START_CMD_PREFIX = bytes.fromhex("000C01")
START_RSP_HEADER = bytes.fromhex("000C0200")

# CRC hesabina giren byte sayisi (CRC alani ve sonrasi haric)
CRC_INPUT_LENGTH = 4


def crc16_modbus(data):
    """CRC-16/MODBUS - polinom 0xA001 (reflected), baslangic 0xFFFF"""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def make_start_response(module_count):
    """00 0C 02 00 <modul sayisi> + CRC (once dusuk, sonra yuksek byte)"""
    body = START_RSP_HEADER + bytes([module_count])
    crc = crc16_modbus(body[:CRC_INPUT_LENGTH])
    return body + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def serve_client(conn, addr, interval, ack, periodic, icd, module_count):
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
                if data[:3] == START_CMD_PREFIX:
                    rsp = make_start_response(module_count)
                    conn.sendall(rsp)
                    print("    TX: %s  (Cihazi Baslat cevabi, modul=%d)"
                          % (hexs(rsp), module_count))
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
    p.add_argument("--modules", type=int, default=5,
                   help="--icd modunda bildirilecek modul sayisi (varsayilan 5)")
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
                             args.icd, args.modules)
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
