/**
 * @file dialogconnect.h
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#ifndef DIALOGCONNECT_H
#define DIALOGCONNECT_H

#include <QDialog>
#include <QtSerialPort/qserialport.h>
#include <QPointer>

namespace Ui {
class DialogConnect;
}

class DialogConnect : public QDialog
{
    Q_OBJECT

public:
    explicit DialogConnect(QSerialPort *serialPort, QWidget *parent);
    ~DialogConnect();

private:
    Ui::DialogConnect *ui;
    QPointer<QSerialPort> pSerialPort = nullptr;
    QList<QSerialPortInfo> infos;

    void refreshSerialPort (void);

Q_SIGNALS:
    void sendEvent (QObject *obj, QEvent *event);

private Q_SLOTS:
    void updateSerialPort(int index);
};

#endif // DIALOGCONNECT_H
