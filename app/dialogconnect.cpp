/**
 * @file dialogconnect.cpp
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#include "dialogconnect.h"
#include <QSerialPortInfo>
#include <QtWidgets/qpushbutton.h>
#include <QKeyEvent>
#include "ui_dialogconnect.h"

static const char blankString[] = QT_TRANSLATE_NOOP("DialogConnect", "N/A");

DialogConnect::DialogConnect(QSerialPort *serialPort, QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogConnect)
    , pSerialPort(serialPort)
{
    ui->setupUi(this);
    setWindowTitle(QString(_TARGET) + " - UART connect");

    QObject::connect(ui->cB_Uart, &QComboBox::currentIndexChanged, this, &DialogConnect::updateSerialPort);
    QObject::connect(ui->cB_Uart, &QComboBox::currentIndexChanged, this, [=, this] (int index) {
        if (ui->cB_Uart->itemText(index) != "TCP_IP") {
            ui->label_Descr->setText(mInfos.at(index).description());
        }
        else {
            ui->label_Descr->setText("");
        }
    });
    refreshSerialPort();
    ui->cB_Uart->setFocus();
}

DialogConnect::~DialogConnect()
{
    delete ui;
}

void DialogConnect::refreshSerialPort() {
    QString CurrentPort = pSerialPort->portName();

    const QList<QSerialPortInfo> unfilteredInfos = QSerialPortInfo::availablePorts();
#if defined(__APPLE__)
    static QRegularExpression portRE("cu\\..*usb.*" ,QRegularExpression::CaseInsensitiveOption); //List only USB COM port
    static QRegularExpression portREall("cu\\..*" ,QRegularExpression::CaseInsensitiveOption); //List cu. port
#elif defined(__linux__)
    static QRegularExpression portRE("tty[AU].*"); //List only USB COM port
    static QRegularExpression portREall("tty\\..*" ,QRegularExpression::CaseInsensitiveOption); //List tty. port
#else
    static QRegularExpression portRE("COM.*");
#endif

    for (const QSerialPortInfo &info : unfilteredInfos) {
        if (QString(info.portName()).contains(portRE)) {
            mInfos.append(info);
        }
    }

#if defined(__APPLE__) || defined(__linux__)
    if (mInfos.empty()) {
        for (const QSerialPortInfo &info : unfilteredInfos) {
            if (QString(info.portName()).contains(portREall)) {
                mInfos.append(info);
            }
        }
    }
#endif

    ui->cB_Uart->clear();
    const QString blankStr = QString::fromUtf8(blankString);

    for (int i = 0; i < mInfos.size(); ++i) {
        // Access the current port info by reference.
        // .at(i) is faster than [] because it doesn't perform bounds checking in release mode.
        const QSerialPortInfo &info = mInfos.at(i);

        const QString description = info.description();
        const QString manufacturer = info.manufacturer();
        const QString serialNumber = info.serialNumber();
        const auto vendorId = info.vendorIdentifier();
        const auto productId = info.productIdentifier();

        // Build the description string for the tooltip.
        // Using reserve() can optimize memory allocation if the string grows significantly.
        QString desc;
        desc.reserve(200);

        desc.append("Description: ");
        desc.append(description.isEmpty() ? blankStr : description);

        desc.append("\nManufacturer: ");
        desc.append(manufacturer.isEmpty() ? blankStr : manufacturer);

        desc.append("\nSerial number: ");
        desc.append(serialNumber.isEmpty() ? blankStr : serialNumber);

        desc.append("\nLocation: ");
        desc.append(info.systemLocation());

        desc.append("\nVendor Identifier: ");
        // vendorId is 0 if not available. Check for non-zero before converting.
        desc.append(vendorId ? QString::number(vendorId, 16) : blankStr);

        desc.append("\nProduct Identifier: ");
        // productId is 0 if not available. Check for non-zero before converting.
        desc.append(productId ? QString::number(productId, 16) : blankStr);

        // Add the port name to the combobox.
        ui->cB_Uart->addItem(info.portName());

        // Set the constructed description as a tooltip for the newly added item.
        // count() - 1 is the index of the last added item.
        ui->cB_Uart->setItemData(ui->cB_Uart->count() - 1, desc, Qt::ToolTipRole);
    }

    //Add TCP_IP Socket
    ui->cB_Uart->addItem("TCP_IP");
    ui->cB_Uart->setItemData(ui->cB_Uart->count() - 1, "TCP_IP Socket Server", Qt::ToolTipRole);

    int CurrentIndex = ui->cB_Uart->findText(CurrentPort);
    if (CurrentIndex >= 0)
        ui->cB_Uart->setCurrentIndex(CurrentIndex);
    else if (!mInfos.isEmpty())
        ui->cB_Uart->setCurrentIndex(0);

}

void DialogConnect::updateSerialPort(int index) {
    pSerialPort->setPortName(ui->cB_Uart->itemText(index));
}
