# ALT SİSTEM ENTEGRASYON TEST ARACI — PROJE PROMPTU

Bu dosya, projeyi sıfırdan kurmak için gereken her şeyi içerir.
Bir modele verildiğinde aynı sonucu üretebilmelidir.

---

## 1. AMAÇ

Elimde, bir alt sistemle TCP üzerinden haberleşen **hazır bir C++ entegrasyon
sınıfı** var (`AltSistem`). Bu sınıfı, fiziksel donanım bağlı değilken
GUI üzerinden test edebilmek istiyorum.

İki ayrı program yazılacak:

1. **TcpHexTool** — Qt tabanlı test aracı. `AltSistem`'i çağırır, sonucu gösterir.
2. **fake_device.py** — sahte alt sistem. Gerçek bir TCP soketi açar,
   protokole uygun cevap verir.

Kritik kural: **`AltSistem` kodu hiç değişmeyecek.** Saha kurulumunda
aynı kod birebir çalışmalı. Simülasyon kod seviyesinde değil, **ağ
seviyesinde** yapılır — `AltSistem` karşısında donanım mı simülatör mü
olduğunu bilmez.

---

## 2. TEKNOLOJİ KISITLARI

**TcpHexTool:**
- Qt 5.7, C++11, Qt Widgets
- qmake (`.pro`) ve CMake, ikisi de çalışsın
- Qt 6 API'si yok, C++14/17/20 yok
- Bloklayan çağrı yok (`waitForConnected`, `waitForReadyRead`, `waitForBytesWritten`)
- Eski usul `SIGNAL()`/`SLOT()` bağlantıları

**fake_device.py:**
- Python 3.6+, **sadece standart kütüphane** (`socket`, `threading`, `tkinter`, `argparse`)
- Harici paket yok, `pip install` yok

---

## 3. MİMARİ SINIRLAR

Bunlar tasarımın omurgası; ihlal edilmemeli.

**GUI protokolü bilmez.** `MainWindow` içinde HEX üretme, CRC hesaplama,
command id, header, byte ofseti, paket ayrıştırma **olmayacak**. Bunların
tamamı `AltSistem`'in sorumluluğudur.

```
GUI  →  AltSistem  →  paket + CRC + TCP + cevap çözümleme  →  Alt Sistem
```

**Tek bağlantı sahibi vardır.** `AltSistem` bağlantıyı `connectToHost` ile
kendisi kurar. `MainWindow` kendi `QTcpSocket`'ini açmaz — iki soket açılırsa
biri diğerinin yerini kapar.

**Simülatör ayrı süreçte çalışır.** Qt uygulamasının içine gömülmez.
Sebepleri: (a) `AltSistem` senkron çağrı yapıyorsa aynı olay döngüsünü
paylaşmak kilitlenme yaratır, (b) gerçek ağ yolu ancak böyle test edilir,
(c) simülatörü kapatmak "kabloyu çekmek" gibi davranır.

**Basitlik önceliklidir.** Design pattern, manager/factory/strategy sınıfları,
protocol engine, state machine, DI, plugin sistemi, veritabanı, JSON,
log framework, gereksiz thread — hiçbiri kullanılmayacak. Gerekmedikçe
yeni sınıf açılmayacak.

**Kalıcı log yok.** Dosyaya/veritabanına yazılmaz. Ekranda görünen bilgi
program kapanınca kaybolabilir.

---

## 4. PROTOKOL (gerçek paketlerle doğrulanmıştır)

### Çerçeve

```
[adres] [komut] [veri boyutu] [veri...] [CRC lo] [CRC hi]
```

| Alan | Açıklama |
|---|---|
| adres | Modül adresi. `0x00` = tüm modüller, `0x01..N` = ilgili modül |
| komut | Komut numarası |
| veri boyutu | **Sadece veri alanındaki** byte sayısı (başlık ve CRC hariç) |
| CRC | CRC-16/MODBUS, **önce düşük byte sonra yüksek byte** |

Toplam paket uzunluğu = `3 + veri boyutu + 2`.

Bu, TCP stream'inde çerçeveleme için yeterlidir: 3. byte okununca paketin
geri kalanının `boyut + 2` byte olduğu bilinir.

### CRC

- Algoritma: **CRC-16/MODBUS**, polinom `0xA001` (reflected), başlangıç `0xFFFF`, son XOR yok
- **CRC girdisi: paketin yalnızca ilk 4 byte'ı.** Sonraki veri byte'ları
  hesaba katılmaz. Bu, 7 byte'lık bir gövdeyle doğrulanmıştır.
- Sonuç pakete **önce düşük byte, sonra yüksek byte** yazılır (Modbus standardı)

### Veri alanı byte sırası

- Durum kelimeleri: **little endian, LSB first** — Bit 0, ilk byte'ın en düşük biti
- Modbus'ta CRC little-endian, veri alanları genelde big-endian'dır;
  bu protokolde ikisi farklıdır, karıştırılmamalı

### Komut 0x0C — Cihazı Başlat / modül sayısı

Bağlantı kurulunca ilk çağrılan komut. Alt sistemdeki toplam modül sayısını verir.

```
İstek : 00 0C 01 00           | C1 B7
Cevap : 00 0C 02 00 05        | C1 47
                     └──┬──┘
                    modül sayısı (16-bit, yüksek byte önce) = 5
```

### Komut 0x0B — Modül durumları

Modül başına 32-bit durum kelimesi. Adres `0x00` ise tüm modüller
(`modül sayısı × 4` byte), aksi halde tek modül (4 byte).

```
İstek : 02 0B 01 00           | 71 CE      2. modülün durumu
Cevap : 02 0B 04 AA BB CC DD  | F2 E1

İstek : 00 0B 01 00           | 70 76      tüm modüller
Cevap : 00 0B 0C <12 byte>    | ....       3 modül varsa
```

Bilgi alma komutlarında istek veri alanı daima tek byte `0x00`'dır.

Durum bitleri (kısmen bilinen):
- Bit 0 = Ana Besleme Normal
- Bit 1 = Dahili olarak Hazır
- Bit 22-23 = hata seviyesi: 0 hata yok, 1 minör, 2 majör, 3 kritik

---

## 5. fake_device.py — SAHTE ALT SİSTEM

Ayrı süreçte çalışan, kendi penceresi olan TCP sunucusu.

### Yapı

- Ağ tarafı kendi thread'inde; `tkinter` **sadece arayüz fonksiyonunda**
  import edilir, böylece `--nogui` modu tkinter kurulu olmayan makinede de çalışır
- Ağ thread'inden arayüze log satırları bir `queue.Queue` üzerinden taşınır
- Her bağlantıya ayrı thread'de hizmet verilir; eşzamanlı birden fazla istemci kabul edilir

### Parametreler — canlı değiştirilebilir

Parametreler **cevap üretilirken** okunur, başlangıçta kopyalanmaz.
Böylece değeri değiştirip tekrar sorgulamak yeterlidir; yeniden başlatma
veya yeniden bağlanma gerekmez.

| Parametre | Açıklama |
|---|---|
| Module Count | 1-5, alt sistemdeki modül sayısı |
| Modül durumları | Modül başına 4 byte, hat üzerindeki sırasıyla HEX girilir |
| Cevap gecikmesi | ms, yavaş cihaz simülasyonu |
| Cevap verme | işaretliyse hiç cevap dönmez (timeout testi) |
| CRC'yi bozuk gönder | işaretliyse CRC kasten bozulur |

`CRC_MODE` dosyanın başında bir sabittir: `1` = ilk 4 byte, `2` = CRC
hariç tüm byte'lar. Gerçek cihaz `1` kullanır; `2` yalnızca karşı tarafın
hangi kuralda olduğunu denemek içindir.

### Arayüz

```
IP: [0.0.0.0 ▼]  Port: [5000]  [BAŞLAT/DURDUR]   Durum
─────────────────────────────────────────────────────
PARAMETRELER
Module Count: [3]  Cevap gecikmesi (ms): [0]
[ ] Cevap verme (timeout testi)   [ ] CRC'yi bozuk gönder
─────────────────────────────────────────────────────
MODÜL DURUMLARI  (hat üzerindeki 4 byte)
Modül 1: [ 11 22 33 44 ]
Modül 2: [ AA BB CC DD ]
...
─────────────────────────────────────────────────────
TRAFİK
[14:02:11] Baglandi: 127.0.0.1:45272
[14:02:12] RX: 00 0C 01 00 C1 B7
[14:02:12] TX: 00 0C 02 00 03 C1 47   (modül sayısı=3)
                                          [TEMİZLE]
```

IP listesi `ip -4 -o addr show` çıktısından doldurulur — loopback alias'ları
ancak böyle görünür; `gethostbyname` yeterli değildir. Liste, açılır menü
her açıldığında yeniden taranır.

`--nogui` modu arayüzsüz çalışır, argümanlarla yapılandırılır.

### Yardımcı betik

`add_device_ip.sh` — gerçek cihazın IP'sini loopback üzerinde geçici olarak
oluşturur (`ip addr add <ip>/32 dev lo`), Ctrl-C'de siler. Simülatörü
**başlatmaz**; arayüz normal kullanıcıyla açılmalıdır (root ile GUI sorun çıkarır).

---

## 6. TcpHexTool — QT TEST ARACI

### Dosyalar

```
TcpHexTool/
├── CMakeLists.txt
├── TcpHexTool.pro
├── main.cpp
├── MainWindow.h / .cpp / .ui
├── AltSistem.h / .cpp        (dışarıdan gelir, değiştirilmez)
└── fake_device.py
```

### Arayüz

```
DEVICE CONTROL
[ START DEVICE ]
──────────────────────────────────────
RESULT
Start:                                SUCCESS
Module Count (alt sistemden okunan):  3
──────────────────────────────────────
MODULE STATUS
Module Address (0 = tüm modüller): [1]
[ GET MODULE STATUS ]
┌────────────────────────────────────┐
└────────────────────────────────────┘
```

Bağlantı alanı yoktur — `AltSistem` adresi kendi içinde tutar ve bağlantıyı
kendisi kurar.

**Module Count bir girdi değil, çıktıdır.** Alt sistemden okunur.
**Module Address bir girdidir** — kullanıcı hangi modülü sorguladığını seçer.

### MainWindow sorumluluğu

Sadece üç şey: kullanıcı girdisini almak, `AltSistem`'i çağırmak, dönen
sonucu göstermek. Başka hiçbir şey.

```cpp
void MainWindow::onStartDeviceClicked()
{
    altSistem->start();
    bool ok = altSistem->isStarted();
    ui->labelStartResult->setText(ok ? "SUCCESS" : "FAILED");
    ui->labelResultModuleCount->setText(
        ok ? QString::number(altSistem->moduleCount()) : QString("-"));
}
```

---

## 7. TEST SENARYOSU

```
1. python3 fake_device.py            (CRC_MODE = 1)
2. Qt Creator'da TcpHexTool'u çalıştır
3. START DEVICE       -> Start: SUCCESS, Module Count: 3
4. Simülatörde Module Count'u 5 yap, tekrar START DEVICE  -> 5
5. "Cevap verme" işaretle, START DEVICE  -> FAILED (timeout)
6. "CRC'yi bozuk gönder" işaretle        -> FAILED (CRC hatası)
7. Module Address = 1, GET MODULE STATUS -> 1. modülün durumu
8. Module Address = 0                    -> tüm modüllerin durumu
```

Doğrulamanın püf noktası: simülatörde ayarladığın değerle GUI'de girdiğin
değeri **farklı** tut. Aynı olurlarsa, aracın cevabı gerçekten okuyup
okumadığını ayırt edemezsin.

---

## 8. BİLİNEN TUZAKLAR

Bunlar bu projede fiilen yaşandı; tekrar edilmemeli.

**CRC kapsamı.** CRC yalnızca ilk 4 byte üzerinden hesaplanır. Tüm gövdeyi
vermek yaygın bir varsayımdır ve bu protokolde yanlıştır. 4 byte'lık
gövdelerde iki kural aynı sonucu verdiği için hata gizlenir; ancak daha
uzun bir cevapta ortaya çıkar.

**CRC byte sırası.** Pakete önce düşük byte yazılır. Okurken de öyle
çözülmelidir. `0x9346` değeri hatta `46 93` olarak görünür.

**Veri alanı byte sırası CRC'ninkinden farklıdır.** Modül sayısı `00 05`
olarak gelir ve 5 demektir. Düşük-byte-önce okunursa 1280 çıkar — bu, aynı
kuralı iki farklı alana uygulamaktan doğan tipik hatadır.

**`char` işaretlidir.** `data[i] << 8` gibi ifadelerde `0x80` üstü değerler
negatife düşer. Her byte erişiminde `(quint8)` dönüşümü yapılmalıdır.

**Veri boyutu alanı tüm çerçeveyi saymaz.** Sadece veri byte'larını sayar.

**İki soket açma.** `AltSistem` kendi bağlantısını kurarken GUI'de ayrıca
CONNECT'e basılırsa, tek bağlantı kabul eden bir cihazda ikincisi reddedilir.

**Qt Creator'da yeni dosya.** `.pro`'ya dosya eklendiğinde **Build → Run qmake**
çalıştırılmalıdır, aksi halde eski Makefile ile derlenir.

**Qt 5.7'de `toHex()` ayırıcı almaz.** `toHex(' ')` Qt 5.9'da eklendi;
5.7'de boşluklar elle eklenmelidir.

**`QAbstractSocket::error` aşırı yüklüdür.** Yeni usul `connect` ile
bağlanırken `static_cast` gerekir; eski usul `SIGNAL()` ile gerekmez.

---

## 9. YAPILMAYACAKLAR

- `AltSistem` kodunu değiştirmek
- GUI'de HEX/CRC/paket üretmek veya ayrıştırmak
- İkinci bir TCP bağlantısı açmak
- Simülatörü Qt uygulamasının içine gömmek
- Simülatörün gerçek cihazda olmayan bir davranışı taklit etmesi
  (örneğin kendiliğinden periyodik veri göndermesi)
- Dosyaya log yazmak, veritabanı, JSON, konfigürasyon dosyası
- Protokol dokümanı olmadan byte anlamı uydurmak
