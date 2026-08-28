#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTimer>

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

private:
    void sendHex();
    void log(const QString &text);

    Ui::MainWindow *ui;
    QTcpSocket *socket;
    QTimer *timer;
};

#endif // MAINWINDOW_H
