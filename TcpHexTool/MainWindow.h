#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTimer>

#include "AltSystemWorker.h"

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSendClicked();
    void onStartAutoClicked();
    void onStopAutoClicked();
    void onClearClicked();

    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError();

    void onTestClicked();
    void onPacketReady(const QByteArray &packet);
    void onTestResult(bool success, const QString &message);

private:
    void sendHex();
    void log(const QString &text);

    Ui::MainWindow *ui;
    QTcpSocket *socket;
    QTimer *timer;
    AltSystemWorker *worker;
};

#endif // MAINWINDOW_H
