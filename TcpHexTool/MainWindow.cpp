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
    connect(ui->btnModuleStatus, SIGNAL(clicked()),
            this, SLOT(onModuleStatusClicked()));
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

// Modul adresi bir GUI girdisi: hangi modulun durumu istendigini
// kullanici secer. 0 = tum moduller.
// Cevabin cozumlenmesi yine AltSistem'in isi; GUI byte gormez.
void MainWindow::onModuleStatusClicked()
{
    int address = ui->spinModuleAddress->value();

    // TODO (AltSistem): asagidaki satirlari kendi fonksiyon adlarinla ac
    //
    //   altSistem->readModuleStatus(address);
    //
    //   ui->textModuleStatus->setPlainText(altSistem->moduleStatusText());

    ui->textModuleStatus->setPlainText(
        QString("AltSistem baglanmadi (istenen adres: %1)").arg(address));
}
