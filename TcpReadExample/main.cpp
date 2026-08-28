// TCP'den gelen veriyi okuyan en kucuk ornek.
// GUI yok, class yok, tek dosya. Sadece baglanir ve gelen byte'lari yazar.
//
// Derle:  qmake && make
// Calistir:  ./TcpReadExample                     -> 127.0.0.1:5000
//            ./TcpReadExample 192.168.1.100 5000

#include <QCoreApplication>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDateTime>
#include <QDebug>

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

    QString host = (argc > 1) ? QString(argv[1]) : QString("127.0.0.1");
    quint16 port = (argc > 2) ? QString(argv[2]).toUShort() : 5000;

    QTcpSocket socket;

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
