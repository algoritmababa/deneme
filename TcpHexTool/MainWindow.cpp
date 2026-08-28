#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QNetworkInterface>
#include <QDateTime>
#include <QHostAddress>

// "AA 55 01 02" veya "AA550102" kabul edilir, hatali ise false doner.
static bool hexToBytes(const QString &text, QByteArray &out)
{
    QString clean = text;
    clean.remove(' ');

    if (clean.isEmpty() || (clean.size() % 2) != 0)
        return false;

    for (int i = 0; i < clean.size(); ++i) {
        QChar c = clean.at(i);
        bool isHexDigit = (c >= '0' && c <= '9') ||
                          (c >= 'a' && c <= 'f') ||
                          (c >= 'A' && c <= 'F');
        if (!isHexDigit)
            return false;
    }

    out = QByteArray::fromHex(clean.toLatin1());
    return true;
}

// Qt 5.7'de toHex() ayirici karakter almadigi icin bosluklari elle ekliyoruz.
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

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    socket = new QTcpSocket(this);
    timer = new QTimer(this);

    // Local IPv4 adreslerini doldur
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (int i = 0; i < addresses.size(); ++i) {
        if (addresses.at(i).protocol() == QAbstractSocket::IPv4Protocol)
            ui->comboLocalIp->addItem(addresses.at(i).toString());
    }

    connect(ui->btnConnect, SIGNAL(clicked()), this, SLOT(onConnectClicked()));
    connect(ui->btnDisconnect, SIGNAL(clicked()), this, SLOT(onDisconnectClicked()));
    connect(ui->btnSend, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(ui->btnStart, SIGNAL(clicked()), this, SLOT(onStartAutoClicked()));
    connect(ui->btnStop, SIGNAL(clicked()), this, SLOT(onStopAutoClicked()));
    connect(ui->btnClear, SIGNAL(clicked()), this, SLOT(onClearClicked()));

    connect(socket, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onSocketError()));

    connect(timer, SIGNAL(timeout()), this, SLOT(onSendClicked()));

    worker = new AltSystemWorker(this);
    connect(ui->btnTest, SIGNAL(clicked()), this, SLOT(onTestClicked()));
    connect(worker, SIGNAL(packetReady(QByteArray)),
            this, SLOT(onPacketReady(QByteArray)));
    connect(worker, SIGNAL(testResult(bool,QString)),
            this, SLOT(onTestResult(bool,QString)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::log(const QString &text)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->textLog->appendPlainText("[" + time + "] " + text);
}

void MainWindow::onConnectClicked()
{
    QString localIp = ui->comboLocalIp->currentText();
    QString targetIp = ui->editTargetIp->text().trimmed();
    quint16 port = ui->editTargetPort->text().trimmed().toUShort();

    socket->abort();

    if (!socket->bind(QHostAddress(localIp), 0)) {
        log("Bind failed: " + socket->errorString());
        return;
    }

    socket->connectToHost(QHostAddress(targetIp), port);
}

void MainWindow::onDisconnectClicked()
{
    socket->disconnectFromHost();
}

void MainWindow::onSendClicked()
{
    sendHex();
}

void MainWindow::sendHex()
{
    QByteArray data;
    if (!hexToBytes(ui->editHex->text(), data)) {
        log("Invalid HEX data");
        return;
    }

    if (socket->state() != QAbstractSocket::ConnectedState) {
        log("Not connected");
        return;
    }

    socket->write(data);
    log("TX: " + bytesToHex(data));
}

void MainWindow::onStartAutoClicked()
{
    int interval = ui->editInterval->text().trimmed().toInt();
    if (interval <= 0) {
        log("Invalid interval");
        return;
    }
    timer->start(interval);
}

void MainWindow::onStopAutoClicked()
{
    timer->stop();
}

void MainWindow::onClearClicked()
{
    ui->textLog->clear();
}

void MainWindow::onConnected()
{
    ui->labelStatus->setText("CONNECTED");
    log("CONNECTED");
}

void MainWindow::onDisconnected()
{
    ui->labelStatus->setText("DISCONNECTED");
    log("DISCONNECTED");
    timer->stop();
}

void MainWindow::onReadyRead()
{
    QByteArray data = socket->readAll();
    log("RX: " + bytesToHex(data));

    ui->labelTestRxValue->setText(bytesToHex(data));
    worker->onRxData(data);
}

// --- Cihazi Baslat testi ---------------------------------------

void MainWindow::onTestClicked()
{
    if (socket->state() != QAbstractSocket::ConnectedState) {
        ui->labelResult->setText("Not connected");
        log("Not connected");
        return;
    }

    ui->labelTestRxValue->setText("-");
    ui->labelResult->setText("Cevap bekleniyor...");
    worker->startDevice();
}

// Worker paketi hazirladi: GUI'de goster ve TCP'den gonder.
void MainWindow::onPacketReady(const QByteArray &packet)
{
    ui->labelPacketValue->setText(bytesToHex(packet));

    // CRC paketin son iki byte'i: once dusuk, sonra yuksek byte.
    ui->labelCrcValue->setText(bytesToHex(packet.right(2)));

    socket->write(packet);
    log("TX: " + bytesToHex(packet));
}

void MainWindow::onTestResult(bool success, const QString &message)
{
    QString text = success ? QString::fromUtf8("✓ CİHAZ BAŞLATILDI")
                           : QString::fromUtf8("✗ CİHAZ BAŞLATILAMADI");
    if (!message.isEmpty())
        text += "  (" + message + ")";

    ui->labelResult->setText(text);
    log("TEST RESULT: " + text);
}

void MainWindow::onSocketError()
{
    ui->labelStatus->setText("DISCONNECTED");
    log("ERROR: " + socket->errorString());
}
