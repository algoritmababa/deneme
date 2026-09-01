#!/usr/bin/env python3
"""Alt sistem simulatoru.

TcpHexTool / AltSistem entegrasyon kodunu, gercek donanim bagli degilken
test etmek icin kullanilir. Gercek bir TCP soketi acar ve Interface
Control Document'taki protokole gore cevap verir; karsi taraf gercek
cihazla konustugunu sanir.

Protokol (dogrulanmis):
    Giden : 00 0C 01 00 | C1 B7
    Gelen : 00 0C 02 00 05 | C1 47
                         ^^ modul sayisi
    CRC-16/MODBUS, sadece ilk 4 byte uzerinden,
    pakete once dusuk sonra yuksek byte olarak yazilir.

Kullanim:
    python3 fake_device.py                 # arayuzlu
    python3 fake_device.py --nogui         # arayuzsuz (tkinter gerekmez)
    python3 fake_device.py --nogui --port 5000 --modules 3

Parametreler cevap URETILIRKEN okunur; arayuzden degistirdigin deger
bir sonraki cevaba aninda yansir, yeniden baslatmak gerekmez.
"""

import argparse
import socket
import subprocess
import threading
import time

# ---------------------------------------------------------------
# Protokol
# ---------------------------------------------------------------

# Komutu ilk 3 byte'tan taniyoruz; 4. byte'i entegrasyon kodu
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


def local_ipv4_addresses():
    """Makinedeki IPv4 adresleri.

    Once `ip -4 -o addr show` ciktisini okur; loopback alias'lari
    (ip addr add ... dev lo) ancak boyle gorunur. O yoksa `hostname -I`,
    o da yoksa makine adini cozmeye duser.

    Liste duzenlenebilir: burada gorunmeyen bir adresi elle yazabilirsin.
    """
    found = ["0.0.0.0", "127.0.0.1"]

    def add(addr):
        addr = addr.strip()
        if addr and addr not in found:
            found.append(addr)

    try:
        out = subprocess.check_output(["ip", "-4", "-o", "addr", "show"],
                                      stderr=subprocess.DEVNULL,
                                      universal_newlines=True)
        for line in out.splitlines():
            parts = line.split()
            # ornek: 1: lo inet 127.0.0.1/8 scope host lo ...
            if "inet" in parts:
                add(parts[parts.index("inet") + 1].split("/")[0])
        return found
    except (OSError, subprocess.SubprocessError, ValueError, IndexError):
        pass

    try:
        out = subprocess.check_output(["hostname", "-I"],
                                      stderr=subprocess.DEVNULL,
                                      universal_newlines=True)
        for addr in out.split():
            add(addr)
        return found
    except (OSError, subprocess.SubprocessError):
        pass

    try:
        for addr in socket.gethostbyname_ex(socket.gethostname())[2]:
            add(addr)
    except OSError:
        pass
    return found


def hexs(data):
    """b'\\x00\\x0c' -> '00 0C'"""
    return " ".join("%02X" % b for b in data)


def make_start_response(module_count, corrupt_crc=False):
    """00 0C 02 00 <modul sayisi> + CRC (once dusuk, sonra yuksek byte)"""
    body = START_RSP_HEADER + bytes([module_count & 0xFF])
    crc = crc16_modbus(body[:CRC_INPUT_LENGTH])
    low, high = crc & 0xFF, (crc >> 8) & 0xFF
    if corrupt_crc:
        low ^= 0xFF
    return body + bytes([low, high])


# ---------------------------------------------------------------
# Simule edilen alt sistemin durumu
# ---------------------------------------------------------------

class DeviceParams(object):
    """Arayuzden yazilir, ag thread'inden okunur. Kilit ile korunur."""

    def __init__(self, module_count=5):
        self._lock = threading.Lock()
        self.module_count = module_count
        self.response_delay_ms = 0
        self.no_response = False
        self.corrupt_crc = False

    def snapshot(self):
        """Cevap uretilecegi an degerlerin tutarli bir kopyasini alir."""
        with self._lock:
            return (self.module_count, self.response_delay_ms,
                    self.no_response, self.corrupt_crc)

    def set(self, **kwargs):
        with self._lock:
            for key, value in kwargs.items():
                setattr(self, key, value)


# ---------------------------------------------------------------
# Ag tarafi - kendi thread'inde calisir
# ---------------------------------------------------------------

class DeviceServer(object):
    def __init__(self, params, log):
        self.params = params
        self.log = log            # log(str) - arayuze ya da ekrana yazar
        self._server = None
        self._thread = None
        self._running = False
        self._busy = False        # gercek cihaz gibi: ayni anda tek istemci

    def is_running(self):
        return self._running

    def start(self, host, port):
        if self._running:
            return False

        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            server.bind((host, port))
            server.listen(1)
        except OSError as exc:
            server.close()
            self.log("HATA: %s" % exc)
            return False

        self._server = server
        self._running = True
        self._thread = threading.Thread(target=self._accept_loop, daemon=True)
        self._thread.start()
        self.log("Dinleniyor: %s:%d" % (host, port))
        return True

    def stop(self):
        if not self._running:
            return
        self._running = False
        self._busy = False
        if self._server is not None:
            self._server.close()      # accept() bu sayede uyanir
            self._server = None
        self.log("Durduruldu")

    def _accept_loop(self):
        # accept() surekli calisir; hizmet ayri bir thread'de verilir.
        # Boylece ikinci bir baglanti isletim sistemi kuyrugunda sessizce
        # beklemek yerine hemen reddedilir ve sebebi loga yazilir.
        while self._running:
            try:
                conn, addr = self._server.accept()
            except OSError:
                return                # stop() soketi kapatti

            if self._busy:
                # Gercek cihaz da ayni anda tek baglanti kabul ediyor.
                self.log("REDDEDILDI: %s:%d - zaten bagli bir istemci var" % addr)
                conn.close()
                continue

            self._busy = True
            self.log("Baglandi: %s:%d" % addr)
            threading.Thread(target=self._session, args=(conn,),
                             daemon=True).start()

    def _session(self, conn):
        try:
            self._serve(conn)
        except (ConnectionResetError, BrokenPipeError, OSError):
            self.log("Baglanti koptu")
        finally:
            conn.close()
            self._busy = False
            self.log("Baglanti kapandi")

    def _serve(self, conn):
        while self._running:
            data = conn.recv(4096)
            if not data:
                return
            self.log("RX: %s" % hexs(data))
            self._respond(conn, data)

    def _respond(self, conn, data):
        if data[:3] != START_CMD_PREFIX:
            self.log("    bilinmeyen komut, cevap yok")
            return

        # Parametreler tam bu anda okunuyor: arayuzde yapilan degisiklik
        # bir sonraki cevapta gecerli olur.
        module_count, delay_ms, no_response, corrupt_crc = self.params.snapshot()

        if no_response:
            self.log("    cevap verilmedi (timeout testi)")
            return

        if delay_ms > 0:
            time.sleep(delay_ms / 1000.0)

        rsp = make_start_response(module_count, corrupt_crc)
        conn.sendall(rsp)
        self.log("TX: %s   (modul=%d%s)"
                 % (hexs(rsp), module_count, ", CRC BOZUK" if corrupt_crc else ""))


# ---------------------------------------------------------------
# Arayuz - tkinter sadece burada import edilir
# ---------------------------------------------------------------

def run_gui(params, host, port):
    import queue
    import tkinter as tk
    from tkinter import ttk

    messages = queue.Queue()

    def log(text):
        messages.put("[%s] %s" % (time.strftime("%H:%M:%S"), text))

    server = DeviceServer(params, log)

    root = tk.Tk()
    root.title("ALT SISTEM SIMULATOR")

    frame = ttk.Frame(root, padding=10)
    frame.pack(fill="both", expand=True)

    # --- baglanti ---
    conn_row = ttk.Frame(frame)
    conn_row.pack(fill="x")

    ttk.Label(conn_row, text="IP:").pack(side="left")
    host_var = tk.StringVar(value=host)
    host_box = ttk.Combobox(conn_row, textvariable=host_var, width=16,
                            values=local_ipv4_addresses())
    # Liste her acildiginda yeniden taranir; simulator acikken eklenen
    # bir adresi gormek icin uygulamayi kapatip acmak gerekmez.
    host_box.configure(postcommand=lambda: host_box.configure(
        values=local_ipv4_addresses()))
    host_box.pack(side="left", padx=(4, 10))

    ttk.Label(conn_row, text="Port:").pack(side="left")
    port_var = tk.StringVar(value=str(port))
    ttk.Entry(conn_row, textvariable=port_var, width=8).pack(side="left", padx=(4, 10))

    status_var = tk.StringVar(value="Durduruldu")
    button_var = tk.StringVar(value="BASLAT")

    def toggle():
        if server.is_running():
            server.stop()
            button_var.set("BASLAT")
            status_var.set("Durduruldu")
        else:
            try:
                p = int(port_var.get())
            except ValueError:
                log("HATA: gecersiz port")
                return
            h = host_var.get().strip()
            if not h:
                log("HATA: IP bos")
                return
            if server.start(h, p):
                button_var.set("DURDUR")
                status_var.set("Dinleniyor: %s:%d" % (h, p))

    ttk.Button(conn_row, textvariable=button_var, command=toggle).pack(side="left")
    ttk.Label(conn_row, textvariable=status_var).pack(side="left", padx=10)

    ttk.Separator(frame).pack(fill="x", pady=8)

    # --- parametreler ---
    ttk.Label(frame, text="PARAMETRELER").pack(anchor="w")

    param_row = ttk.Frame(frame)
    param_row.pack(fill="x", pady=4)

    ttk.Label(param_row, text="Module Count:").pack(side="left")
    module_var = tk.IntVar(value=params.module_count)
    ttk.Spinbox(param_row, from_=1, to=5, width=5, textvariable=module_var,
                command=lambda: params.set(module_count=module_var.get())
                ).pack(side="left", padx=(4, 16))

    ttk.Label(param_row, text="Cevap gecikmesi (ms):").pack(side="left")
    delay_var = tk.IntVar(value=params.response_delay_ms)
    ttk.Spinbox(param_row, from_=0, to=10000, increment=100, width=7,
                textvariable=delay_var,
                command=lambda: params.set(response_delay_ms=delay_var.get())
                ).pack(side="left", padx=4)

    check_row = ttk.Frame(frame)
    check_row.pack(fill="x")

    no_rsp_var = tk.BooleanVar(value=params.no_response)
    ttk.Checkbutton(check_row, text="Cevap verme (timeout testi)",
                    variable=no_rsp_var,
                    command=lambda: params.set(no_response=no_rsp_var.get())
                    ).pack(side="left", padx=(0, 16))

    crc_var = tk.BooleanVar(value=params.corrupt_crc)
    ttk.Checkbutton(check_row, text="CRC'yi bozuk gonder",
                    variable=crc_var,
                    command=lambda: params.set(corrupt_crc=crc_var.get())
                    ).pack(side="left")

    ttk.Separator(frame).pack(fill="x", pady=8)

    # --- trafik ---
    ttk.Label(frame, text="TRAFIK").pack(anchor="w")

    text = tk.Text(frame, height=14, width=60, state="disabled")
    text.pack(fill="both", expand=True, pady=4)

    def clear():
        text.configure(state="normal")
        text.delete("1.0", "end")
        text.configure(state="disabled")

    ttk.Button(frame, text="TEMIZLE", command=clear).pack(anchor="e")

    def safe_int(var, fallback):
        try:
            return int(var.get())
        except (ValueError, tk.TclError):
            return fallback

    # Spinbox'a elle yazilan degerler de gecerli olsun (ok tuslari disinda).
    module_var.trace_add("write",
        lambda *_: params.set(module_count=safe_int(module_var, params.module_count)))
    delay_var.trace_add("write",
        lambda *_: params.set(response_delay_ms=safe_int(delay_var, params.response_delay_ms)))

    # Ag thread'inden gelen satirlari arayuze tasi.
    def drain():
        while True:
            try:
                line = messages.get_nowait()
            except queue.Empty:
                break
            text.configure(state="normal")
            text.insert("end", line + "\n")
            text.see("end")
            text.configure(state="disabled")
        root.after(100, drain)

    def on_close():
        server.stop()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.after(100, drain)

    toggle()          # acilir acilmaz dinlemeye basla
    root.mainloop()


# ---------------------------------------------------------------

def run_headless(params, host, port):
    def log(text):
        print("[%s] %s" % (time.strftime("%H:%M:%S"), text))

    server = DeviceServer(params, log)
    if not server.start(host, port):
        return 1

    print("Ctrl-C ile cik")
    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        server.stop()
    return 0


def main():
    p = argparse.ArgumentParser(description="Alt sistem simulatoru")
    p.add_argument("--host", default="0.0.0.0", help="dinlenecek adres")
    p.add_argument("--port", type=int, default=5000, help="dinlenecek port")
    p.add_argument("--modules", type=int, default=5,
                   help="baslangictaki modul sayisi (varsayilan 5)")
    p.add_argument("--nogui", action="store_true",
                   help="arayuzsuz calistir (tkinter gerekmez)")
    args = p.parse_args()

    params = DeviceParams(module_count=args.modules)

    if args.nogui:
        return run_headless(params, args.host, args.port)

    try:
        run_gui(params, args.host, args.port)
    except ImportError:
        print("tkinter bulunamadi. Kurulum: sudo apt install python3-tk")
        print("Ya da arayuzsuz calistir: python3 fake_device.py --nogui")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
