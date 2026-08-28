#include "AltSystemWorker.h"

// ---------------------------------------------------------------
// Protokol sabitleri - Interface Control Document'tan
// ---------------------------------------------------------------

// Cevap beklerken kullanilan sure
static const int RESPONSE_TIMEOUT_MS = 3000;

// CRC hesabina dahil edilen byte sayisi.
// Hem TX hem RX icin ilk 4 byte kullaniliyor; CRC alaninin kendisi
// ve sonrasindaki payload byte'lari hesaba katilmiyor.
static const int CRC_INPUT_LENGTH = 4;

// Cihazi Baslat komutu (alt sistemdeki toplam modul sayisini ister)
//   Giden : 00 0C 01 00 | C1 B7
static const char START_CMD_BYTES[] = { 0x00, 0x0C, 0x01, 0x00 };

// Beklenen cevabin sabit bolumu
//   Gelen : 00 0C 02 00 05 | C1 47
//                        ^^ modul sayisi
static const char START_RSP_BYTES[] = { 0x00, 0x0C, 0x02, 0x00 };

// Cevap uzunlugu: 4 sabit byte + 1 modul sayisi + 2 CRC
static const int RESPONSE_LENGTH = 7;

// Modul sayisinin cevap icindeki konumu.
// TODO: Interface Control Document ile dogrulanacak. Elimizdeki tek
// ornek cevapta (00 0C 02 00 05 C1 47) modul sayisi olabilecek tek
// byte bu; dokuman gelince teyit edilmeli.
static const int MODULE_COUNT_INDEX = 4;

// ---------------------------------------------------------------

static QString bytesToHex(const QByteArray &data)
{
    QString hex = QString::fromLatin1(data.toHex()).toUpper();

    QString result;
    for (int i = 0; i < hex.size(); i += 2) {
        if (!result.isEmpty())
            result += ' ';
        result += hex.mid(i, 2);
    }
    return result;
}

// CRC-16/MODBUS - polinom 0xA001 (reflected), baslangic 0xFFFF
static quint16 calculateModbusCrc(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    for (int i = 0; i < data.size(); ++i) {
        crc ^= (quint8)data.at(i);
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

// CRC'yi pakete ekler: once dusuk byte, sonra yuksek byte.
static void appendCrc(QByteArray &packet)
{
    quint16 crc = calculateModbusCrc(packet.left(CRC_INPUT_LENGTH));
    packet.append((char)(crc & 0xFF));
    packet.append((char)((crc >> 8) & 0xFF));
}

static QByteArray createStartDevicePacket()
{
    QByteArray packet(START_CMD_BYTES, sizeof(START_CMD_BYTES));
    appendCrc(packet);
    return packet;
}

// Cevabi degerlendirir. message icine kullaniciya gosterilecek metni yazar.
static bool checkStartDeviceResponse(const QByteArray &data, QString &message)
{
    if (data.size() < RESPONSE_LENGTH) {
        message = QString("Cevap kisa (%1 byte, beklenen %2)")
                      .arg(data.size()).arg(RESPONSE_LENGTH);
        return false;
    }

    QByteArray rsp = data.left(RESPONSE_LENGTH);

    QByteArray expectedHeader(START_RSP_BYTES, sizeof(START_RSP_BYTES));
    if (rsp.left(CRC_INPUT_LENGTH) != expectedHeader) {
        message = "Beklenmeyen cevap: " + bytesToHex(rsp.left(CRC_INPUT_LENGTH))
                  + " (beklenen " + bytesToHex(expectedHeader) + ")";
        return false;
    }

    quint16 crc = calculateModbusCrc(rsp.left(CRC_INPUT_LENGTH));
    quint8 crcLow = (quint8)rsp.at(RESPONSE_LENGTH - 2);
    quint8 crcHigh = (quint8)rsp.at(RESPONSE_LENGTH - 1);

    if (crcLow != (quint8)(crc & 0xFF) || crcHigh != (quint8)((crc >> 8) & 0xFF)) {
        message = QString("CRC hatali (gelen %1 %2, beklenen %3 %4)")
                      .arg(crcLow, 2, 16, QChar('0')).arg(crcHigh, 2, 16, QChar('0'))
                      .arg(crc & 0xFF, 2, 16, QChar('0')).arg((crc >> 8) & 0xFF, 2, 16, QChar('0'))
                      .toUpper();
        return false;
    }

    quint8 moduleCount = (quint8)rsp.at(MODULE_COUNT_INDEX);
    message = QString("Modul sayisi: %1").arg(moduleCount);
    return true;
}

// ---------------------------------------------------------------

AltSystemWorker::AltSystemWorker(QObject *parent) :
    QObject(parent),
    waiting(false)
{
    timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, SIGNAL(timeout()), this, SLOT(onTimeout()));
}

void AltSystemWorker::startDevice()
{
    waiting = true;
    timeoutTimer->start(RESPONSE_TIMEOUT_MS);
    emit packetReady(createStartDevicePacket());
}

void AltSystemWorker::onRxData(const QByteArray &data)
{
    if (!waiting)
        return;

    waiting = false;
    timeoutTimer->stop();

    QString message;
    bool ok = checkStartDeviceResponse(data, message);
    emit testResult(ok, message);
}

void AltSystemWorker::onTimeout()
{
    waiting = false;
    emit testResult(false, "Cihaz baslatma cevabi alinamadi (timeout)");
}
