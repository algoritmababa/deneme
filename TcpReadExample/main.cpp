// TCP'den gelen veriyi okuyan en kucuk ornek.
// GUI yok, class yok, tek dosya. Sadece baglanir ve gelen byte'lari yazar.
//
// Baglanti bilgilerini asagidaki sabitlerden degistir.
//
// Qt Creator'da .pro dosyasini acip Run'a basman yeterli.

#include <QCoreApplication>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDateTime>
#include <QDebug>

// ---------------------------------------------------------------
// AYARLAR - baglanti bilgilerini buradan degistir
// ---------------------------------------------------------------

// Baglanilacak cihazin adresi ve portu
static const char *TARGET_IP   = "192.168.1.100";
static const quint16 TARGET_PORT = 5000;

// Cikis yapilacak kendi ag kartinin adresi.
// Bos birakirsan isletim sistemi dogru karti kendisi secer;
// birden fazla ag karti varsa buraya dogru olani yaz.
static const char *LOCAL_IP = "";

// ---------------------------------------------------------------

// Qt 5.7'de toHex() ayirici almadigi icin bosluklari elle ekliyoruz.
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

static QString now()
{
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString host = QString(TARGET_IP);
    quint16 port = TARGET_PORT;

    QTcpSocket socket;

    QString localIp = QString(LOCAL_IP);
    if (!localIp.isEmpty()) {
        if (!socket.bind(QHostAddress(localIp), 0)) {
            qDebug("[%s] BIND FAILED (%s): %s", qPrintable(now()),
                   qPrintable(localIp), qPrintable(socket.errorString()));
            return 1;
        }
        qDebug("[%s] Local IP: %s", qPrintable(now()), qPrintable(localIp));
    }

    QObject::connect(&socket, &QTcpSocket::connected, [&]() {
        qDebug("[%s] CONNECTED", qPrintable(now()));

        // Baglanir baglanmaz bir seyler gondermek istersen:
        // socket.write(QByteArray::fromHex("AA550102FF1000"));
    });

    QObject::connect(&socket, &QTcpSocket::readyRead, [&]() {
        QByteArray data = socket.readAll();
        qDebug("[%s] RX: %s", qPrintable(now()), qPrintable(bytesToHex(data)));
    });

    QObject::connect(&socket, &QTcpSocket::disconnected, [&]() {
        qDebug("[%s] DISCONNECTED", qPrintable(now()));
        QCoreApplication::quit();
    });

    // error() sinyali Qt 5.7'de asiri yuklu oldugu icin static_cast gerekiyor.
    QObject::connect(&socket,
        static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
        [&](QAbstractSocket::SocketError) {
            qDebug("[%s] ERROR: %s", qPrintable(now()), qPrintable(socket.errorString()));
            QCoreApplication::quit();
        });

    qDebug("[%s] Baglaniliyor: %s:%u", qPrintable(now()), qPrintable(host), port);
    socket.connectToHost(QHostAddress(host), port);

    return app.exec();
}
