#ifndef ALTSYSTEMWORKER_H
#define ALTSYSTEMWORKER_H

#include <QObject>
#include <QByteArray>
#include <QTimer>

// "Cihazi Baslat" testini yurutur.
// TCP baglantisinin sahibi degildir; paketi MainWindow gonderir,
// gelen cevabi da MainWindow buraya iletir.
class AltSystemWorker : public QObject
{
    Q_OBJECT

public:
    explicit AltSystemWorker(QObject *parent = 0);

    void startDevice();

signals:
    void testResult(bool success, const QString &message);
    void packetReady(const QByteArray &packet);

public slots:
    void onRxData(const QByteArray &data);

private slots:
    void onTimeout();

private:
    QTimer *timeoutTimer;
    bool waiting;
};

#endif // ALTSYSTEMWORKER_H
