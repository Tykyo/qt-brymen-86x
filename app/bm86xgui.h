/**
 * @file bm86xgui.h
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#ifndef BM86XGUI_H
#define BM86XGUI_H

#include <QLabel>
#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include <QtNetwork>
#include <QtWidgets/qpushbutton.h>
#include <QSettings>
#include "bm86xdecode.h"
#include "bm86xplot.h"
#include "datastorage.h"
#include "shortcutaction.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class BM86Xgui;
}
QT_END_NAMESPACE

class BM86Xgui : public QMainWindow
{
    Q_OBJECT

public:
    explicit BM86Xgui (QWidget *parent = nullptr);
    ~BM86Xgui() override;

    struct IconParam
    {
        QString path;
        QSize   size;
    };


    // Load settings from file into UI components
    void readSettings();

    // Save current UI state to configuration file
    void saveSettings();

private:
    Ui::BM86Xgui*       ui;
    BM86x_ValueTypeDef  mDmmValue;
    BM86xDataType_s     mDmmData;
    QSerialPort         mSerialPort;
    QTcpSocket          mTcpSocket;
    QByteArray          mRxBuffer;
    QPointer<QSettings> mSettings = nullptr;

    bool mDeviceConnected = false;
    bool mUseTcpSocket    = false;
    bool mPausePlot      = false;
    bool mRecordData      = false;
    bool mExcludeLastRow  = false;
    bool clearSettings    = false;

    QString mTcpQuery     = "192.168.1.1";
    QString mTcpHostName  = "";
    QString mTcpHostIP    = "";

    QColor mColorMain     = QColor(255, 204, 0);
    QColor mColorAux      = QColor(0, 255, 255);
    QColor mColorXAxis    = QColor(125, 125, 125);
    QColor mColorMousePos = QColor(125, 125, 125);
    int    currentScale   = BM86xPlot::Minutes_1;

    QTimer mTimoutTimer;
    QTimer mSerialConnectTimer;
    QTimer mTestLCDTimer;

    QPointer<DataStorage>    mStorageWindow;
    QList<Shortcut::Action>  mActionShortcutList;
    QList<QPointer<QAction>> mListActionAquisitionRate;
    QStringList              mModeStringList;

    QList<QPixmap>   mIconListSymMain;
    QList<QPixmap>   mIconListSymAux;
    QList<QPixmap>   mIconListUnitMain;
    QList<QPixmap>   mIconListUnitAux;
    QList<QPixmap>   mIconListDigitMain;
    QList<QPixmap>   mIconListDigitAux;
    QList<IconParam> mIconParamListSymMain;
    QList<IconParam> mIconParamListSymAux;
    QList<IconParam> mIconParamListUnitMain;
    QList<IconParam> mIconParamListUnitAux;
    QList<IconParam> mIconParamListDigitMain;
    QList<IconParam> mIconParamListDigitAux;

    QPixmap combinePixmap        (const QList<QPixmap>& pixmapList, const QSize& size);
    void    displayLCD           (const BM86xDataType_s& data);
    QString getAppConfigPath     ();
    void    initLCD              ();
    void    print                ();
    void    renderPlot           (QPaintDevice *device, const QRectF &documentRect);
    void    renderPlot           (QPaintDevice *device);
    void    savePDF              (const QString& file);
    void    savePNG              (const QString& file);
    void    saveSVG              (const QString& file);
    QPixmap setColoredSvg        (const QString& svgPath, const QColor& color, const QSize& size);
    void    setColorAux          (const QColor& color);
    void    setColorMain         (const QColor& color);
    void    setColorMousePos     (const QColor& color);
    void    setColorXAxis        (const QColor& color);
    void    testLCD              (const int& time);
    bool    waitForHostReachable (const QString& ip);

private Q_SLOTS:
    void onCbAuxChecked            (bool val);
    void onCbMainChecked           (bool val);
    void onChangePlotScale         (const int& scale);
    void onChangePlotStyle         (const int& type);
    void onChangeReadSpeed         (const int& index);
    void onChooseColorAux          ();
    void onChooseColorMain         ();
    void onChooseColorMousePos     ();
    void onChooseColorXAxis        ();
    void onConnectMultimeter       (bool val = false);
    void onConnectMultimeterSerial ();
    void onDisconnectMultimeter    (bool val = false);
    void onDataReceived            (const BM86xDataType_s& data);
    void onPausePlot               ();
    void onReadSerialPort          ();
    void onRestoreLCD              ();
    void onSavePlot                ();
    void onSerialPortError         (const QSerialPort::SerialPortError& error);
    void onShowHelp                ();
    void onTcpDisconnect           ();
    void onTestLCD                 ();
    void onTimeOut                 ();

Q_SIGNALS:
    void dataReceived (const BM86xDataType_s& data);
    void colorChanged (const DataStorage::filterColor_s& color);
};

namespace BM86x {
enum delay { BLOCK_DELAY_MS = 500 };

enum symMain {
    sym_main_ALL,
    sym_main_AC,
    sym_main_Battery,
    sym_main_continuity,
    sym_main_Crest,
    sym_main_DC,
    sym_main_Delta,
    sym_main_Hold,
    sym_main_minus,
    sym_main_bar_minus,
    sym_main_Peak_Avg,
    sym_main_Peak_Max,
    sym_main_Peak_Min,
    sym_main_Range_Auto,
    sym_main_Rec,
    sym_main_T_minus,
    sym_main_T1,
    sym_main_T2,
    sym_main_VFD,

    sym_main_LAST
};

enum unitMain {
    unit_main_ALL,
    unit_main_ampere,
    unit_main_db,
    unit_main_duty,
    unit_main_fara,
    unit_main_hz,
    unit_main_kilo,
    unit_main_mega,
    unit_main_micro,
    unit_main_millis,
    unit_main_nano,
    unit_main_ohm,
    unit_main_siemens,
    unit_main_volt,

    unit_main_LAST
};

enum symAux {
    sym_aux_ALL,
    sym_aux_AC,
    sym_aux_minus,
    sym_aux_T2,

    sym_aux_LAST
};

enum unitAux {
    unit_aux_ALL,
    unit_aux_ampere,
    unit_aux_hz,
    unit_aux_kilo,
    unit_aux_mega,
    unit_aux_micro,
    unit_aux_millis,
    unit_aux_p420ma,
    unit_aux_volt,

    unit_aux_LAST
};

enum digitMain {

    digit_main_1e,
    digit_main_1f,
    digit_main_1a,
    digit_main_1d,
    digit_main_1c,
    digit_main_1g,
    digit_main_1b,

    digit_main_1p,
    digit_main_2e,
    digit_main_2f,
    digit_main_2a,
    digit_main_2d,
    digit_main_2c,
    digit_main_2g,
    digit_main_2b,

    digit_main_2p,
    digit_main_3e,
    digit_main_3f,
    digit_main_3a,
    digit_main_3d,
    digit_main_3c,
    digit_main_3g,
    digit_main_3b,

    digit_main_3p,
    digit_main_4e,
    digit_main_4f,
    digit_main_4a,
    digit_main_4d,
    digit_main_4c,
    digit_main_4g,
    digit_main_4b,

    digit_main_4p,
    digit_main_5e,
    digit_main_5f,
    digit_main_5a,
    digit_main_5d,
    digit_main_5c,
    digit_main_5g,
    digit_main_5b,

    digit_main_6e,
    digit_main_6f,
    digit_main_6a,
    digit_main_6d,
    digit_main_6c,
    digit_main_6g,
    digit_main_6b,
    digit_main_ALL,

    digit_main_LAST
};

enum digitAux {
    digit_aux_7e,
    digit_aux_7f,
    digit_aux_7a,
    digit_aux_7d,
    digit_aux_7c,
    digit_aux_7g,
    digit_aux_7b,

    digit_aux_7p,
    digit_aux_8e,
    digit_aux_8f,
    digit_aux_8a,
    digit_aux_8d,
    digit_aux_8c,
    digit_aux_8g,
    digit_aux_8b,

    digit_aux_8p,
    digit_aux_9e,
    digit_aux_9f,
    digit_aux_9a,
    digit_aux_9d,
    digit_aux_9c,
    digit_aux_9g,
    digit_aux_9b,

    digit_aux_9p,
    digit_aux_10e,
    digit_aux_10f,
    digit_aux_10a,
    digit_aux_10d,
    digit_aux_10c,
    digit_aux_10g,
    digit_aux_10b,
    digit_aux_ALL,

    digit_aux_LAST
};
}
#endif // BM86XGUI_H
