#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

// TODO (AltSistem): entegrasyon header'ini burada include et
// #include "AltSistem.h"

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void onStartDeviceClicked();

private:
    Ui::MainWindow *ui;

    // TODO (AltSistem): entegrasyon nesnesini burada tut
    // AltSistem *altSistem;
};

#endif // MAINWINDOW_H
