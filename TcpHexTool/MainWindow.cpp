#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // TODO (AltSistem): nesneyi burada olustur
    // altSistem = new AltSistem();

    connect(ui->btnStartDevice, SIGNAL(clicked()),
            this, SLOT(onStartDeviceClicked()));
}

MainWindow::~MainWindow()
{
    // TODO (AltSistem): parent almadigi icin elle sil
    // delete altSistem;

    delete ui;
}

// GUI protokolu bilmez: burada HEX, CRC, header, command id yoktur.
// Baglantiyi da AltSistem kendisi kurar.
// Modul sayisi bir girdi degil, alt sistemden okunan sonuctur.
void MainWindow::onStartDeviceClicked()
{
    // TODO (AltSistem): asagidaki satirlari kendi getter adlarinla ac
    //
    //   altSistem->start();
    //
    //   bool ok = altSistem->isStarted();
    //   ui->labelStartResult->setText(ok ? "SUCCESS" : "FAILED");
    //   ui->labelResultModuleCount->setText(
    //       ok ? QString::number(altSistem->moduleCount()) : QString("-"));

    ui->labelStartResult->setText("AltSistem baglanmadi");
    ui->labelResultModuleCount->setText("-");
}
