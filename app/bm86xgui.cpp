/**
 * @file bm86xgui.cpp
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */


#include <QColorDialog>
#include <QFileDialog>
#include <QFont>
#include <QFontDatabase>
#include <QInputDialog>
#include <QPageLayout>
#include <QPainter>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QStandardPaths>
#include <QSvgGenerator>
#include <QSvgRenderer>
#include <cmath>
#include "bm86xgui.h"
#include "bm86xcommon.h"
#include "dialogconnect.h"
#include "ui_bm86xgui.h"
#include "helpwindow.h"

#if !defined(SERIAL_CONNECT_DELAY)
#ifdef Q_OS_WIN
#define SERIAL_CONNECT_DELAY 0
#else
#define SERIAL_CONNECT_DELAY 2000
#endif
#elif defined(Q_OS_WIN)
#undef SERIAL_CONNECT_DELAY
#define SERIAL_CONNECT_DELAY 0
#endif

#if !defined(BM_TCP_PORT)
#define BM_TCP_PORT 3333
#endif

#define BIT_CHECK(var, bit)  (((var) & (1U << (bit))) != 0) /**< Macro for checking if a bit is set */

constexpr int resolution = 150;
constexpr double PlotRenderMargin = 100.0;

BM86Xgui::BM86Xgui(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::BM86Xgui)
{
    ui->setupUi(this);
    setWindowTitle(_TARGET);
    statusBar()->showMessage("Not connected");

    int fontId = QFontDatabase::addApplicationFont(":/fonts/consolas_bold");

    if (fontId != -1) {
        QString fontFamily = QFontDatabase::applicationFontFamilies(fontId).at(0);
        QFont customFont(fontFamily, 13);
        ui->progressBar->setFont(customFont);
        ui->cBSpeed->setFont(customFont);
        ui->cBplot->setFont(customFont);
    }

    /* {QString(text),QString(toolTip),QAction(*action),QKeySequence(shortcut)} */
    mActionShortcutList.append({
        "Quit",
        "Close application",
        ui->actionQuit,
        QKeySequence::Quit
    });
    mActionShortcutList.append({
        "Connect",
        "Connect to device",
        ui->actionConnect,
        QKeySequence::Open
    });
    mActionShortcutList.append({
        "Disconnect",
        "Disconnect of the device",
        ui->actionDisconnect,
        Qt::ControlModifier | Qt::Key_D
    });
    mActionShortcutList.append({
        "Color main",
        "Set main data color",
        ui->actionColor_main,
        Qt::ControlModifier | Qt::Key_1
    });

    mActionShortcutList.append({
        "Color aux",
        "Set aux data color",
        ui->actionColor_aux,
        Qt::ControlModifier | Qt::Key_2
    });

    mActionShortcutList.append({
        "Color X axis",
        "Set X axis color",
        ui->actionColor_X_Axis,
        Qt::ControlModifier | Qt::Key_3
    });

    mActionShortcutList.append({
        "Color mouse coordinates",
        "Set mouse position color",
        ui->actionColor_Mouse_Pos,
        Qt::ControlModifier | Qt::Key_4
    });

    mActionShortcutList.append({
        "Save",
        "Save Plot",
        ui->actionSave_Plot,
        QKeySequence::Save
    });

    mActionShortcutList.append({
        "Export",
        "Export Data",
        ui->actionExport,
        Qt::ControlModifier | Qt::Key_E
    });

    mActionShortcutList.append({
        "Antialiasing",
        "Set the Antialiasing of plot",
        ui->actionAntialiasing,
        Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_A
    });

    mActionShortcutList.append({
        "Review",
        "Open Review Data",
        ui->actionReview,
        Qt::ControlModifier | Qt::Key_R
    });

    mActionShortcutList.append({
        "Pause",
        "Pause Plot",
        ui->actionPause_Plot,
        Qt::ControlModifier | Qt::Key_Z
    });

    mActionShortcutList.append({
        "Clear",
        "Clear Plot",
        ui->actionClear_Plot,
        QKeySequence::Delete
    });

    mActionShortcutList.append({
        "Clear",
        "Clear Data",
        ui->actionClear,
        QKeySequence::Backspace
    });

    mActionShortcutList.append({
        "Record",
        "Record Data",
        ui->actionRecord,
        Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_R
    });

    mActionShortcutList.append({
        "Print",
        "Print Plot",
        ui->actionPrint_Plot,
        QKeySequence::Print
    });

    mActionShortcutList.append({
        "Print",
        "Print Data",
        ui->actionPrint_Data,
        Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_P
    });
    mActionShortcutList.append({
        "Test LCD",
        "Test LCD",
        ui->actionTest_LCD,
        Qt::ControlModifier | Qt::Key_T
    });
    mActionShortcutList.append({
        "Help",
        "Show Help",
        ui->actionHelp,
        QKeySequence::HelpContents
    });

    for (auto &item : mActionShortcutList)
        item.apply();

    // Update button tooltip with associated QAction shortcut
    QList<Shortcut::ButtonParam> buttonList;
    buttonList.append({ui->pB_SavePlot, ui->actionSave_Plot});
    buttonList.append({ui->pB_ShowData, ui->actionReview});
    buttonList.append({ui->pB_Clear, ui->actionClear_Plot});
    buttonList.append({ui->pB_Pause, ui->actionPause_Plot});
    buttonList.append({ui->pB_Record, ui->actionRecord});

    for (auto &item : buttonList) {
        QString currentTooltip = item.button->toolTip();
        QString currentActionShortcut = item.action->shortcut().toString(QKeySequence::NativeText);
        QString finalToolTip = QString("%1\n%2").arg(currentTooltip, currentActionShortcut);
        item.button->setToolTip(finalToolTip);
    }

    // We use this workaround to pass QAction shortcut for the DataStorage QButton tooltips
    // It's not the best
    QList<QPointer<QAction>> actionList;
    actionList.append(ui->actionExport);
    actionList.append(ui->actionClear);
    actionList.append(ui->actionRecord);

    mStorageWindow = new DataStorage(actionList, this, mExcludeLastRow);
    QObject::connect(this, &BM86Xgui::colorChanged, mStorageWindow, &DataStorage::onSetColorFilter);
    QObject::connect(ui->actionExport, &QAction::triggered, mStorageWindow, &DataStorage::onExportData);
    QObject::connect(ui->actionHelp, &QAction::triggered, this, &BM86Xgui::onShowHelp);
    QObject::connect(ui->actionClear, &QAction::triggered, mStorageWindow, &DataStorage::onClearData);
    QObject::connect(ui->actionPrint_Data, &QAction::triggered, mStorageWindow, &DataStorage::onPrintData);
    QObject::connect(mStorageWindow, &DataStorage::showStatusChanged, ui->actionReview, &QAction::setChecked);
    QObject::connect(mStorageWindow, &DataStorage::filterDataChanged, ui->qtPlot, &BM86xQwtPlot::onFilterDataChanged);
    QObject::connect(ui->pB_ShowData, &QPushButton::clicked, this, [=, this]() {
        mStorageWindow->onShowWindow(!mStorageWindow->isVisible());
    });
    QObject::connect(ui->actionReview, &QAction::triggered, this, [=, this]() {
        mStorageWindow->onShowWindow(!mStorageWindow->isVisible());
    });
    QObject::connect(mStorageWindow, &DataStorage::recordDataChanged, this, [this](bool value) {
        mRecordData = value;
        if (mRecordData) {
            ui->pB_Record->setIcon(setColoredSvg(":/image/Record_Data_Stop", QColor(229, 128, 255), ui->pB_Record->iconSize()));
        }
        else {
            ui->pB_Record->setIcon(setColoredSvg(":/image/Record_Data", QColor(229, 128, 255), ui->pB_Record->iconSize()));
        }
    });

    QObject::connect(ui->cBMain, &QCheckBox::clicked, this, &BM86Xgui::onCbMainChecked);
    QObject::connect(ui->cBAux, &QCheckBox::clicked, this, &BM86Xgui::onCbAuxChecked);
    ui->cBMain->setChecked(true);
    ui->cBAux->setChecked(true);

    mTimoutTimer.setSingleShot(false);
    mTimoutTimer.setInterval(2000);
    QObject::connect(&mTimoutTimer, &QTimer::timeout, this, &BM86Xgui::onTimeOut);

    mTestLCDTimer.setSingleShot(true);
    mTestLCDTimer.setInterval(2000);
    QObject::connect(&mTestLCDTimer, &QTimer::timeout, this, &BM86Xgui::onRestoreLCD);

    /*
        Open the serial port on the Arduino USB serial, reset the device.
        So we need to wait for the device to reset, minimum 2000msec.
        On windows, it's seem to not be needed.
    */
    mSerialConnectTimer.setInterval(SERIAL_CONNECT_DELAY);
    mSerialConnectTimer.setSingleShot(true);
    QObject::connect(&mSerialConnectTimer, &QTimer::timeout, this, &BM86Xgui::onConnectMultimeterSerial);

    mSerialPort.setFlowControl(QSerialPort::NoFlowControl);
    mSerialPort.setBaudRate(QSerialPort::Baud115200);
    mSerialPort.setDataBits(QSerialPort::Data8);
    mSerialPort.setParity(QSerialPort::NoParity);
    mSerialPort.setStopBits(QSerialPort::OneStop);

    QObject::connect(ui->actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);
    QObject::connect(ui->actionConnect, &QAction::triggered, this, &BM86Xgui::onConnectMultimeter);
    QObject::connect(ui->actionDisconnect, &QAction::triggered, this, &BM86Xgui::onDisconnectMultimeter);
    QObject::connect(ui->actionColor_main, &QAction::triggered, this, &BM86Xgui::onChooseColorMain);
    QObject::connect(ui->actionColor_aux, &QAction::triggered, this, &BM86Xgui::onChooseColorAux);
    QObject::connect(ui->actionColor_X_Axis, &QAction::triggered, this, &BM86Xgui::onChooseColorXAxis);
    QObject::connect(ui->actionColor_Mouse_Pos, &QAction::triggered, this, &BM86Xgui::onChooseColorMousePos);
    QObject::connect(ui->actionSave_Plot, &QAction::triggered, this, &BM86Xgui::onSavePlot);
    QObject::connect(ui->actionPause_Plot, &QAction::triggered, ui->pB_Pause, &QPushButton::click);
    QObject::connect(ui->actionClear_Plot, &QAction::triggered, ui->pB_Clear, &QPushButton::click);
    QObject::connect(ui->actionRecord, &QAction::triggered, ui->pB_Record, &QPushButton::click);
    QObject::connect(ui->actionPrint_Plot, &QAction::triggered, this, &BM86Xgui::print);
    QObject::connect(ui->actionTest_LCD, &QAction::triggered, this, &BM86Xgui::onTestLCD);
    QObject::connect(ui->actionAntialiasing, &QAction::triggered, this, [=, this] (bool value) {
        ui->qtPlot->setAntialiasing(value);
    });
    QObject::connect(ui->actionClear_Settings, &QAction::triggered, this, [=, this] () {
        clearSettings = true;
    });
    QObject::connect(ui->actionCurve, &QAction::triggered, this, [=, this] () {
        onChangePlotStyle(BM86xPlot::PLOT_CURVE);
    });
    QObject::connect(ui->actionLine, &QAction::triggered, this, [=, this] () {
        onChangePlotStyle(BM86xPlot::PLOT_LINE);
    });
    QObject::connect(ui->actionScatter, &QAction::triggered, this, [=, this] () {
        onChangePlotStyle(BM86xPlot::PLOT_SCATTER);
    });
    QObject::connect(ui->actionFullScale, &QAction::triggered, this, [=, this] () {
        onChangePlotScale(BM86xPlot::ScaleFull);
        currentScale = BM86xPlot::ScaleFull;
    });
    QObject::connect(ui->action5_Seconds, &QAction::triggered, this, [=, this] () {
        onChangePlotScale(BM86xPlot::Seconds_5);
        currentScale = BM86xPlot::Seconds_5;
    });
    QObject::connect(ui->action10_Seconds, &QAction::triggered, this, [=, this] () {
        onChangePlotScale(BM86xPlot::Seconds_10);
        currentScale = BM86xPlot::Seconds_10;
    });
    QObject::connect(ui->action30_Seconds, &QAction::triggered, this, [=, this] () {
        onChangePlotScale(BM86xPlot::Seconds_30);
        currentScale = BM86xPlot::Seconds_30;
    });
    QObject::connect(ui->action1_Minute, &QAction::triggered, this, [=, this] () {
        onChangePlotScale(BM86xPlot::Minutes_1);
        currentScale = BM86xPlot::Minutes_1;
    });
    QObject::connect(ui->action5_Minutes, &QAction::triggered, this, [=, this] () {
        onChangePlotScale(BM86xPlot::Minutes_5);
        currentScale = BM86xPlot::Minutes_5;
    });
    QObject::connect(ui->action10_Minutes, &QAction::triggered, this, [=, this] () {
        onChangePlotScale(BM86xPlot::Minutes_10);
        currentScale = BM86xPlot::Minutes_10;
    });
    QObject::connect(ui->action30_Minutes, &QAction::triggered, this, [=, this] () {
        onChangePlotScale(BM86xPlot::Minutes_30);
        currentScale = BM86xPlot::Minutes_30;
    });
    QObject::connect(ui->action1_hour, &QAction::triggered, this, [=, this] () {
        onChangePlotScale(BM86xPlot::Minutes_60);
        currentScale = BM86xPlot::Minutes_60;
    });

    // Set QAction ShortcutContext
    ui->actionConnect->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionDisconnect->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionColor_main->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionColor_aux->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionSave_Plot->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionExport->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionAntialiasing->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionPause_Plot->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionClear_Plot->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionClear->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionRecord->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionReview->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionPrint_Plot->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionPrint_Data->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionColor_Mouse_Pos->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionTest_LCD->setShortcutContext(Qt::ApplicationShortcut);

    QObject::connect(ui->pB_SavePlot, &QPushButton::clicked, this, &BM86Xgui::onSavePlot);
    QObject::connect(ui->pB_Clear, &QPushButton::clicked, ui->qtPlot, &BM86xQwtPlot::onClearData);
    QObject::connect(ui->cBSpeed, &QComboBox::currentIndexChanged, this, &BM86Xgui::onChangeReadSpeed);
    QObject::connect(ui->cBplot, &QComboBox::activated, this, &BM86Xgui::onChangePlotStyle);
    QObject::connect(this, &BM86Xgui::dataReceived, this, &BM86Xgui::onDataReceived);
    QObject::connect(this, &BM86Xgui::dataReceived, mStorageWindow, &DataStorage::onAppendData);
    QObject::connect(this, &BM86Xgui::dataReceived, ui->qtPlot, &BM86xQwtPlot::onAppendData);
    QObject::connect(ui->pB_Pause, &QPushButton::clicked, this, &BM86Xgui::onPausePlot);
    QObject::connect(ui->qtPlot, &BM86xQwtPlot::pausePlot, this, &BM86Xgui::onPausePlot);
    QObject::connect(&mTcpSocket, &QTcpSocket::connected, this, []() {
        qDebug() << "CONNECTED";
    });
    QObject::connect(&mTcpSocket, &QTcpSocket::errorOccurred, this, [this](auto e) {
        (void) e;
        qDebug() << "ERROR:" << mTcpSocket.errorString();
    });
    QObject::connect(ui->pB_Record, &QPushButton::clicked, this, [=, this]() {
        mRecordData = !mRecordData;
        mStorageWindow->onRecordData(mRecordData);
        if (mRecordData) {
            ui->pB_Record->setIcon(setColoredSvg(":/image/Record_Data_Stop", QColor(229, 128, 255), ui->pB_Record->iconSize()));
        }
        else {
            ui->pB_Record->setIcon(setColoredSvg(":/image/Record_Data", QColor(229, 128, 255), ui->pB_Record->iconSize()));
        }
    });    

    // Set default scale
    onChangePlotScale(currentScale);

    QIcon m_icon(":/image/main_icon");
    this->setWindowIcon(m_icon);

    QString buttonStyleSheet = QString(
        "QPushButton {"
        "    border: none;"
        "    background-color: rgba(0,0,0,0);"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(200, 200, 200, 30);"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(150, 150, 150, 80);"
        "}"
    );

    ui->pB_SavePlot->setText("");
    ui->pB_ShowData->setText("");
    ui->pB_Clear->setText("");
    ui->pB_Pause->setText("");
    ui->pB_Record->setText("");
    ui->pB_SavePlot->setIcon(setColoredSvg(":/image/Save_plot", QColor(128, 255, 179), ui->pB_SavePlot->iconSize()));
    ui->pB_ShowData->setIcon(setColoredSvg(":/image/Display_Data", QColor(85, 153, 255), ui->pB_ShowData->iconSize()));
    ui->pB_Clear->setIcon(setColoredSvg(":/image/Clear_Plot", QColor(128, 255, 179), ui->pB_Clear->iconSize()));
    ui->pB_Pause->setIcon(setColoredSvg(":/image/Pause", QColor(255, 127, 42), ui->pB_Pause->iconSize()));
    ui->pB_Record->setIcon(setColoredSvg(":/image/Record_Data", QColor(229, 128, 255), ui->pB_Record->iconSize()));
    ui->pB_SavePlot->setStyleSheet(buttonStyleSheet);
    ui->pB_ShowData->setStyleSheet(buttonStyleSheet);
    ui->pB_Clear->setStyleSheet(buttonStyleSheet);
    ui->pB_Pause->setStyleSheet(buttonStyleSheet);
    ui->pB_Record->setStyleSheet(buttonStyleSheet);

    ui->cBplot->setItemIcon(0,setColoredSvg(":/image/Curve", QColor(0, 255, 102),ui->cBplot->iconSize()));
    ui->cBplot->setItemIcon(1,setColoredSvg(":/image/Line", QColor(128, 128, 255),ui->cBplot->iconSize()));
    ui->cBplot->setItemIcon(2,setColoredSvg(":/image/Scatter", QColor(255, 128, 229),ui->cBplot->iconSize()));

    // Set read speed
    for (int i=0; i < last_speed; i++) {
        ui->cBSpeed->addItem(ReadSpeedTimeText[i]);
    }

    initLCD();

    // Settings configuration
    // 1. Determine the standard path for application data
    // On Windows: C:\Users\<User>\AppData\Local\<AppName>
    // On macOS: ~/Library/Application Support/<AppName>
    // On Linux: ~/.local/share/<AppName>
    QString configDirPath = getAppConfigPath();
    QString settingsFilePath;

    // 2. Ensure the directory exists
    QDir dir(configDirPath);
    if (!dir.exists()) {
        // mkpath creates the directory and any necessary parent directories
        if (!dir.mkpath(".")) {
            qCritical() << "Failed to create configuration directory:" << configDirPath;
            goto END;
        }
    }

    // 3. Define the settings file path
    // We use QFileInfo to correctly handle path separators across OSes
    settingsFilePath = QFileInfo(dir, "BM86x.ini").absoluteFilePath();

    // 4. Initialize QSettings
    // Passing the file path explicitly forces INI format regardless of OS defaults
    mSettings = new QSettings(settingsFilePath, QSettings::IniFormat, this);

    // Check if the settings file is accessible for writing
    if (!mSettings->isWritable()) {
        qWarning() << "Warning: Configuration file is not writable:" << settingsFilePath;
    }

    // read the configuration and apply
    readSettings();
END:
    testLCD(2000);
}

BM86Xgui::~BM86Xgui()
{
    if (clearSettings == false) {
        saveSettings();
    }
    else {
        mSettings->clear();
        mSettings->sync();
    }
    onDisconnectMultimeter();
    delete ui;
}

void BM86Xgui::readSettings()
{
    if (!mSettings) return;

    // Helper lambda to safely read a string value with a default fallback
    auto readValue = [](QSettings *s, const QString &key, const QString &defaultValue) -> QString {
        return s->value(key, defaultValue).toString();
    };

    // Read hardware settings
    mSerialPort.setPortName      ( readValue( mSettings, "Hardware/SerialPort", "COM1" ) );
    mTcpQuery = readValue        ( mSettings, "Hardware/TcpHost", "192.168.1.1" );
    ui->cBSpeed->setCurrentIndex ( QString( readValue( mSettings, "Hardware/Speed",   "0" ) ).toInt() );

    // Read plot settings
    onChangePlotScale
        ( QString( readValue( mSettings, "Plot/Scale", QString::number(BM86xPlot::Minutes_1) ) ).toInt() );
    ui->cBplot->setCurrentIndex
        ( QString( readValue( mSettings, "Plot/Type", QString::number(BM86xPlot::PLOT_LINE)  ) ).toInt() );
    ui->actionAntialiasing->setChecked
        ( QString( readValue( mSettings, "Plot/Antialiasing", "1" ) ).toInt() );
    ui->cBMain->setChecked
        ( QString( readValue( mSettings, "Plot/Main_vis", "1" ) ).toInt() );
    ui->cBAux->setChecked
        ( QString( readValue( mSettings, "Plot/Aux_vis", "1" ) ).toInt() );
    ui->actionB_W_Save_Print->setChecked
        ( QString( readValue( mSettings, "Plot/BW", "0" ) ).toInt() );

    // Read Color mSettings
    setColorMain     ( QColor( readValue( mSettings, "Color/main",  "#FFCC00" ) ) );
    setColorAux      ( QColor( readValue( mSettings, "Color/aux",   "#00FFFF" ) ) );
    setColorXAxis    ( QColor( readValue( mSettings, "Color/axis",  "#7D7D7D" ) ) );
    setColorMousePos ( QColor( readValue( mSettings, "Color/mouse", "#7D7D7D" ) ) );

    // Read Datastorage settings
    DataStorage::config_s config;
    config.isSoundActive     = QString( readValue( mSettings, "Filter/soundActive", "0"   ) ).toInt();
    config.soundVolume       = QString( readValue( mSettings, "Filter/volume",      "10"  ) ).toInt();
    config.filter.mainActive = QString( readValue( mSettings, "Filter/mainActive",  "0"   ) ).toInt();
    config.filter.auxActive  = QString( readValue( mSettings, "Filter/auxActive",   "0"   ) ).toInt();
    config.filter.inverted   = QString( readValue( mSettings, "Filter/inverted",    "0"   ) ).toInt();
    config.filter.main.max   = QString( readValue( mSettings, "Filter/mainMax",     "500" ) ).toDouble();
    config.filter.main.min   = QString( readValue( mSettings, "Filter/mainMin",     "0"   ) ).toDouble();
    config.filter.aux.max    = QString( readValue( mSettings, "Filter/auxMax",      "500" ) ).toDouble();
    config.filter.aux.min    = QString( readValue( mSettings, "Filter/auxMin",      "0"   ) ).toDouble();

    mStorageWindow->setConfig(config);
}

void BM86Xgui::saveSettings()
{
    if (!mSettings || !mSettings->isWritable()) {
        qCritical() << "Cannot save settings: QSettings is not initialized or not writable.";
        return;
    }

    // Helper lambda to save a value only if it differs from the current stored value
    auto saveValue = [](QSettings *s, const QString &key, const QString &value) {
        QString currentValue = s->value(key).toString();

        // Optimization: Avoid unnecessary writes if the value hasn't changed
        if (currentValue != value) {
            s->setValue(key, value);
            qInfo() << "Saved setting:" << key << "=" << value;
        }
    };

    // Save hardware settings
    saveValue( mSettings, "Hardware/SerialPort", mSerialPort.portName() );
    saveValue( mSettings, "Hardware/TcpHost",    mTcpHostName.isEmpty() ? (mTcpHostIP.isEmpty() ? mTcpQuery : mTcpHostIP) : mTcpHostName);
    saveValue( mSettings, "Hardware/Speed",      QString::number( ui->cBSpeed->currentIndex() ) );

    // Save plot settings
    saveValue( mSettings, "Plot/Scale",          QString::number( ui->qtPlot->getPlotScale() ) );
    saveValue( mSettings, "Plot/Type",           QString::number( ui->qtPlot->getPlotType() ) );
    saveValue( mSettings, "Plot/Antialiasing",   QString::number( ui->qtPlot->isAntialiasing() ) );
    saveValue( mSettings, "Plot/Main_vis",       QString::number( ui->qtPlot->isMainVisible() ) );
    saveValue( mSettings, "Plot/Aux_vis",        QString::number( ui->qtPlot->isAuxVisible() ) );
    saveValue( mSettings, "Plot/BW",             QString::number( ui->actionB_W_Save_Print->isChecked() ) );

    // Save Color
    saveValue( mSettings, "Color/main",          mColorMain.name() );
    saveValue( mSettings, "Color/aux",           mColorAux.name() );
    saveValue( mSettings, "Color/axis",          mColorXAxis.name() );
    saveValue( mSettings, "Color/mouse",         mColorMousePos.name() );

    // Save Datastorage
    DataStorage::config_s config = mStorageWindow->getConfig();
    saveValue( mSettings, "Filter/soundActive",  QString::number( config.isSoundActive ) );
    saveValue( mSettings, "Filter/volume",       QString::number( config.soundVolume ) );
    saveValue( mSettings, "Filter/mainActive",   QString::number( config.filter.mainActive ) );
    saveValue( mSettings, "Filter/auxActive",    QString::number( config.filter.auxActive ) );
    saveValue( mSettings, "Filter/inverted",     QString::number( config.filter.inverted ) );
    saveValue( mSettings, "Filter/mainMax",      QString::number( config.filter.main.max ) );
    saveValue( mSettings, "Filter/mainMin",      QString::number( config.filter.main.min ) );
    saveValue( mSettings, "Filter/auxMax",       QString::number( config.filter.aux.max ) );
    saveValue( mSettings, "Filter/auxMin",       QString::number( config.filter.aux.min ) );

    // Force synchronization to ensure data is written to disk immediately
    mSettings->sync();
}

QPixmap BM86Xgui::combinePixmap(const QList<QPixmap> &pixmapList, const QSize &size)
{
    if (pixmapList.isEmpty()) {
        return QPixmap(size);
    }

    // Create a transparent image with the target size
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    for (const QPixmap &pixmap : pixmapList) {
        if (pixmap.isNull()) {
            continue;
        }
        painter.drawPixmap(0, 0, pixmap);
    }

    painter.end();

    return QPixmap::fromImage(image);
}

void BM86Xgui::displayLCD(const BM86xDataType_s &data)
{

    if (mTestLCDTimer.isActive()) {
        return;
    }

    QList<QPixmap> list;
    /* Set Value Main */
    // byte 3 : bit 1-7
    for (int i = 1; i < 8; ++i) {
        if (BIT_CHECK(data.value.rawData[2],i)) {
            list.append(mIconListDigitMain[i - 1]);
        }
    }
    // byte 4-7 : bit 0-7
    for (int byte = 3; byte < 7; ++byte) {
        for (int i = 0; i < 8; ++i) {
            if (BIT_CHECK(data.value.rawData[byte],i)) {
                list.append(mIconListDigitMain[i + (8 * (byte - 2)) - 1 ]);
            }
        }
    }
    // byte 8 : bit 1-7
    for (int i = 1; i < 8; ++i) {
        if (BIT_CHECK(data.value.rawData[7],i)) {
            list.append(mIconListDigitMain[i + (8 * (7 - 2)) - 2 ]);
        }
    }

    if (list.empty()) {
        ui->lcdValue_main->clear();
    }
    else {
        ui->lcdValue_main->setPixmap( combinePixmap(list,
                                                   mIconListDigitMain[BM86x::digit_main_ALL].size()) );
        list.clear();
    }

    /* Set Value Aux */
    // byte 10 : bit 1-7
    for (int i = 1; i < 8; ++i) {
        if (BIT_CHECK(data.value.rawData[9],i)) {
            list.append(mIconListDigitAux[i - 1]);
        }
    }
    // byte 11-13 : bit 0-7
    for (int byte = 10; byte < 13; ++byte) {
        for (int i = 0; i < 8; ++i) {
            if (BIT_CHECK(data.value.rawData[byte],i)) {
                list.append(mIconListDigitAux[i + (8 * (byte - 9)) - 1 ]);
            }
        }
    }

    if (list.empty()) {
        ui->lcdValue_aux->clear();
    }
    else {
        ui->lcdValue_aux->setPixmap( combinePixmap(list,
                                                  mIconListDigitAux[BM86x::digit_aux_ALL].size()) );
        list.clear();
    }

    //Set Range Main
    if (data.value.range == Auto) {
        ui->label_Range->setPixmap(mIconListSymMain[BM86x::sym_main_Range_Auto]);
    }
    else {
        ui->label_Range->clear();
    }

    //Set Peak Mode
    if (data.value.peakMode == All) {
        list.append(mIconListSymMain[BM86x::sym_main_Peak_Max]);
        list.append(mIconListSymMain[BM86x::sym_main_Peak_Min]);
        list.append(mIconListSymMain[BM86x::sym_main_Peak_Avg]);
        ui->label_Peak->setPixmap( combinePixmap(list,
                                                mIconListSymMain[BM86x::sym_main_Peak_Max].size()) );
        list.clear();
    }
    else if (data.value.peakMode == Max) {
        ui->label_Peak->setPixmap(mIconListSymMain[BM86x::sym_main_Peak_Max]);
    }
    else if (data.value.peakMode == Min) {
        ui->label_Peak->setPixmap(mIconListSymMain[BM86x::sym_main_Peak_Min]);
    }
    else if (data.value.peakMode == Avg) {
        ui->label_Peak->setPixmap(mIconListSymMain[BM86x::sym_main_Peak_Avg]);
    }
    else {
        ui->label_Peak->clear();
    }

    //Set Rec Mode
    if (data.value.rec == Rec) {
        ui->label_Rec->setPixmap(mIconListSymMain[BM86x::sym_main_Rec]);
    }
    else if (data.value.rec == Crest) {
        ui->label_Rec->setPixmap(mIconListSymMain[BM86x::sym_main_Crest]);
    }
    else if (data.value.rec == Hold) {
        ui->label_Rec->setPixmap(mIconListSymMain[BM86x::sym_main_Hold]);
    }
    else if (data.value.rec == HoldRec) {
        list.append(mIconListSymMain[BM86x::sym_main_Hold]);
        list.append(mIconListSymMain[BM86x::sym_main_Rec]);
        ui->label_Rec->setPixmap(combinePixmap(list, mIconListSymMain[BM86x::sym_main_Hold].size()));
        list.clear();
    }
    else if (data.value.rec == HoldCrest) {
        list.append(mIconListSymMain[BM86x::sym_main_Hold]);
        list.append(mIconListSymMain[BM86x::sym_main_Crest]);
        ui->label_Rec->setPixmap(combinePixmap(list, mIconListSymMain[BM86x::sym_main_Hold].size()));
        list.clear();
    }
    else {
        ui->label_Rec->clear();
    }

    // Set Battery
    if (data.value.low_bat == true) {
        ui->label_Battery->setPixmap(mIconListSymMain[BM86x::sym_main_Battery]);
    }
    else {
        ui->label_Battery->clear();
    }

    // Set Bar Graph
    if (data.value.visible_bar == true)
    {
        ui->progressBar->setHidden(false);
        int bar_value = data.count / 1250;

        // Check if 500,000 count, we check if digit 6 display something
        if (data.value.rawData[7] >> 1 != 0) {
            bar_value /= 10;
        }

        if (bar_value >= ui->progressBar->maximum())
            bar_value = ui->progressBar->maximum();

        if (data.value.m_overflow == true) {
            bar_value = ui->progressBar->maximum(); // 50 max
        }

        ui->progressBar->setValue(bar_value);
        if (data.value.isNegative == true ) {
            ui->label_Bar_sign->setPixmap(mIconListSymMain[BM86x::sym_main_bar_minus]);
        } else {
            ui->label_Bar_sign->clear();
        }
    }
    else
    {
        ui->label_Bar_sign->clear();
        ui->progressBar->setHidden(true);
    }

    /* Set Main Symbol */
    // DC
    if (BIT_CHECK(data.value.rawData[0],4)) {
        list.append(mIconListSymMain[BM86x::sym_main_DC]);
    }
    // AC
    if (BIT_CHECK(data.value.rawData[1],0)) {
        list.append(mIconListSymMain[BM86x::sym_main_AC]);
    }
    // T1
    if (BIT_CHECK(data.value.rawData[1],1)) {
        list.append(mIconListSymMain[BM86x::sym_main_T1]);
    }
    // T-
    if (BIT_CHECK(data.value.rawData[1],2)) {
        list.append(mIconListSymMain[BM86x::sym_main_T_minus]);
    }
    // T2
    if (BIT_CHECK(data.value.rawData[1],3)) {
        list.append(mIconListSymMain[BM86x::sym_main_T2]);
    }
    // -
    if (BIT_CHECK(data.value.rawData[1],7)) {
        list.append(mIconListSymMain[BM86x::sym_main_minus]);
    }
    // VFD
    if (BIT_CHECK(data.value.rawData[1],6)) {
        list.append(mIconListSymMain[BM86x::sym_main_VFD]);
    }
    // Delta
    if (BIT_CHECK(data.value.rawData[2],0)) {
        list.append(mIconListSymMain[BM86x::sym_main_Delta]);
    }

    if (list.empty()) {
        ui->label_Mode_main->clear();
    }
    else {
        ui->label_Mode_main->setPixmap( combinePixmap(list,
                                                     mIconListSymMain[BM86x::sym_main_ALL].size()) );
        list.clear();
    }

    /* Set Main Unit   */
    // %D
    if (BIT_CHECK(data.value.rawData[14],7)) {
        list.append(mIconListUnitMain[BM86x::unit_main_duty]);
    }
    // k
    if (BIT_CHECK(data.value.rawData[14],6)) {
        list.append(mIconListUnitMain[BM86x::unit_main_kilo]);
    }
    // M
    if (BIT_CHECK(data.value.rawData[14],5)) {
        list.append(mIconListUnitMain[BM86x::unit_main_mega]);
    }
    // ohm
    if (BIT_CHECK(data.value.rawData[14],4)) {
        list.append(mIconListUnitMain[BM86x::unit_main_ohm]);
    }
    // Hz
    if (BIT_CHECK(data.value.rawData[14],0)) {
        list.append(mIconListUnitMain[BM86x::unit_main_hz]);
    }
    // dB
    if (BIT_CHECK(data.value.rawData[14],1)) {
        list.append(mIconListUnitMain[BM86x::unit_main_db]);
    }
    // m
    if (BIT_CHECK(data.value.rawData[14],2)) {
        list.append(mIconListUnitMain[BM86x::unit_main_millis]);
    }
    // µ
    if (BIT_CHECK(data.value.rawData[14],3)) {
        list.append(mIconListUnitMain[BM86x::unit_main_micro]);
    }
    // V
    if (BIT_CHECK(data.value.rawData[7],0)) {
        list.append(mIconListUnitMain[BM86x::unit_main_volt]);
    }
    // A
    if (BIT_CHECK(data.value.rawData[13],7)) {
        list.append(mIconListUnitMain[BM86x::unit_main_ampere]);
    }
    // n
    if (BIT_CHECK(data.value.rawData[13],6)) {
        list.append(mIconListUnitMain[BM86x::unit_main_nano]);
    }
    // F
    if (BIT_CHECK(data.value.rawData[13],5)) {
        list.append(mIconListUnitMain[BM86x::unit_main_fara]);
    }
    //S
    if (BIT_CHECK(data.value.rawData[13],4)) {
        list.append(mIconListUnitMain[BM86x::unit_main_siemens]);
    }

    if (list.empty()) {
        ui->label_Unit_main->clear();
    }
    else {
        ui->label_Unit_main->setPixmap( combinePixmap(list,
                                                     mIconListUnitMain[BM86x::unit_main_ALL].size()) );
        list.clear();
    }

    /* Set Aux Symbol  */
    // T2
    if (BIT_CHECK(data.value.rawData[8],6)) {
        list.append(mIconListSymAux[BM86x::sym_aux_T2]);
    }
    // AC
    if (BIT_CHECK(data.value.rawData[8],5)) {
        list.append(mIconListSymAux[BM86x::sym_aux_AC]);
    }
    // -
    if (BIT_CHECK(data.value.rawData[8],4)) {
        list.append(mIconListSymAux[BM86x::sym_aux_minus]);
    }
    // Continuity
    if (BIT_CHECK(data.value.rawData[9],0)) {
        list.append(mIconListSymMain[BM86x::sym_main_continuity]);
    }

    if (list.empty()) {
        ui->label_Mode_aux->clear();
    }
    else {
        ui->label_Mode_aux->setPixmap( combinePixmap(list,
                                                    mIconListSymAux[BM86x::sym_aux_ALL].size()) );
        list.clear();
    }

    /* Set Aux Unit    */
    // M
    if (BIT_CHECK(data.value.rawData[13],0)) {
        list.append(mIconListUnitAux[BM86x::unit_aux_mega]);
    }
    // k
    if (BIT_CHECK(data.value.rawData[13],1)) {
        list.append(mIconListUnitAux[BM86x::unit_aux_kilo]);
    }
    // Hz
    if (BIT_CHECK(data.value.rawData[13],2)) {
        list.append(mIconListUnitAux[BM86x::unit_aux_hz]);
    }
    // µ
    if (BIT_CHECK(data.value.rawData[8],0)) {
        list.append(mIconListUnitAux[BM86x::unit_aux_micro]);
    }
    // m
    if (BIT_CHECK(data.value.rawData[8],1)) {
        list.append(mIconListUnitAux[BM86x::unit_aux_millis]);
    }
    // A
    if (BIT_CHECK(data.value.rawData[8],2)) {
        list.append(mIconListUnitAux[BM86x::unit_aux_ampere]);
    }
    // V
    if (BIT_CHECK(data.value.rawData[13],3)) {
        list.append(mIconListUnitAux[BM86x::unit_aux_volt]);
    }
    // %4~20mA
    if (BIT_CHECK(data.value.rawData[8],3)) {
        list.append(mIconListUnitAux[BM86x::unit_aux_p420ma]);
    }

    if (list.empty()) {
        ui->label_Unit_aux->clear();
    }
    else {
        ui->label_Unit_aux->setPixmap( combinePixmap(list,
                                                    mIconListUnitAux[BM86x::unit_aux_ALL].size()) );
        list.clear();
    }
}

QString BM86Xgui::getAppConfigPath()
{
    // Use Qt's standard path service to get the correct location for the OS
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    // Fallback if QStandardPaths fails (rare, but good practice)
    if (path.isEmpty()) {
        path = QDir::homePath() + "/.config/BM86Xgui";
    }

    return path;
}

void BM86Xgui::initLCD() {
    mIconParamListSymMain = {
        {":/main_symbol/sym_main_ALL",ui->label_Mode_main->size()},
        {":/main_symbol/sym_main_AC",ui->label_Mode_main->size()},
        {":/main_symbol/sym_main_Battery",ui->label_Battery->size()},
        {":/main_symbol/sym_main_continuity",ui->label_Mode_aux->size()},
        {":/main_symbol/sym_main_Crest",ui->label_Rec->size()},
        {":/main_symbol/sym_main_DC",ui->label_Mode_main->size()},
        {":/main_symbol/sym_main_Delta",ui->label_Mode_main->size()},
        {":/main_symbol/sym_main_Hold",ui->label_Rec->size()},
        {":/main_symbol/sym_main_minus",ui->label_Mode_main->size()},
        {":/main_symbol/sym_main_bar_minus",ui->label_Bar_sign->size()},
        {":/main_symbol/sym_main_Peak_Avg",ui->label_Peak->size()},
        {":/main_symbol/sym_main_Peak_Max",ui->label_Peak->size()},
        {":/main_symbol/sym_main_Peak_Min",ui->label_Peak->size()},
        {":/main_symbol/sym_main_Range_Auto",ui->label_Range->size()},
        {":/main_symbol/sym_main_Rec",ui->label_Rec->size()},
        {":/main_symbol/sym_main_T_minus",ui->label_Mode_main->size()},
        {":/main_symbol/sym_main_T1",ui->label_Mode_main->size()},
        {":/main_symbol/sym_main_T2",ui->label_Mode_main->size()},
        {":/main_symbol/sym_main_VFD",ui->label_Mode_main->size()},

        {":/image/empty",ui->label_Mode_main->size()},   // For safety
    };

    mIconParamListUnitMain = {
        {":/main_unit/unit_main_ALL",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_ampere",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_db",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_duty",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_fara",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_hz",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_kilo",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_mega",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_micro",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_millis",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_nano",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_ohm",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_siemens",ui->label_Unit_main->size()},
        {":/main_unit/unit_main_volt",ui->label_Unit_main->size()},

        {":/image/empty",ui->label_Mode_main->size()},   // For safety
    };

    mIconParamListSymAux = {
        {":/aux_symbol/sym_aux_ALL",ui->label_Mode_aux->size()},
        {":/aux_symbol/sym_aux_AC",ui->label_Mode_aux->size()},
        {":/aux_symbol/sym_aux_minus",ui->label_Mode_aux->size()},
        {":/aux_symbol/sym_aux_T2",ui->label_Mode_aux->size()},

        {":/image/empty",ui->label_Mode_aux->size()},
    };

    mIconParamListUnitAux = {
        {":/aux_unit/unit_aux_ALL",ui->label_Unit_aux->size()},
        {":/aux_unit/unit_aux_ampere",ui->label_Unit_aux->size()},
        {":/aux_unit/unit_aux_hz",ui->label_Unit_aux->size()},
        {":/aux_unit/unit_aux_kilo",ui->label_Unit_aux->size()},
        {":/aux_unit/unit_aux_mega",ui->label_Unit_aux->size()},
        {":/aux_unit/unit_aux_micro",ui->label_Unit_aux->size()},
        {":/aux_unit/unit_aux_millis",ui->label_Unit_aux->size()},
        {":/aux_unit/unit_aux_p420ma",ui->label_Unit_aux->size()},
        {":/aux_unit/unit_aux_volt",ui->label_Unit_aux->size()},

        {":/image/empty",ui->label_Mode_aux->size()},
    };

    mIconParamListDigitMain = {
        {":/main_digit/digit_main_1e",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_1f",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_1a",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_1d",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_1c",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_1g",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_1b",ui->lcdValue_main->size()},

        {":/main_digit/digit_main_1p",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_2e",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_2f",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_2a",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_2d",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_2c",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_2g",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_2b",ui->lcdValue_main->size()},

        {":/main_digit/digit_main_2p",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_3e",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_3f",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_3a",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_3d",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_3c",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_3g",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_3b",ui->lcdValue_main->size()},

        {":/main_digit/digit_main_3p",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_4e",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_4f",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_4a",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_4d",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_4c",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_4g",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_4b",ui->lcdValue_main->size()},

        {":/main_digit/digit_main_4p",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_5e",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_5f",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_5a",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_5d",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_5c",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_5g",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_5b",ui->lcdValue_main->size()},

        {":/main_digit/digit_main_6e",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_6f",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_6a",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_6d",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_6c",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_6g",ui->lcdValue_main->size()},
        {":/main_digit/digit_main_6b",ui->lcdValue_main->size()},

        {":/main_digit/digit_main_ALL",ui->lcdValue_main->size()},

        {":/image/empty",ui->lcdValue_main->size()},
    };

    mIconParamListDigitAux = {
        {":/aux_digit/digit_aux_7e",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_7f",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_7a",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_7d",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_7c",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_7g",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_7b",ui->lcdValue_aux->size()},

        {":/aux_digit/digit_aux_7p",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_8e",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_8f",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_8a",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_8d",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_8c",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_8g",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_8b",ui->lcdValue_aux->size()},

        {":/aux_digit/digit_aux_8p",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_9e",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_9f",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_9a",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_9d",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_9c",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_9g",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_9b",ui->lcdValue_aux->size()},

        {":/aux_digit/digit_aux_9p",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_10e",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_10f",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_10a",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_10d",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_10c",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_10g",ui->lcdValue_aux->size()},
        {":/aux_digit/digit_aux_10b",ui->lcdValue_aux->size()},

        {":/aux_digit/digit_aux_ALL",ui->lcdValue_aux->size()},

        {":/image/empty",ui->lcdValue_aux->size()},
    };

    // Generate MAIN Symbol
    for (auto &item : mIconParamListSymMain) {
        mIconListSymMain.append(setColoredSvg(item.path, mColorMain, item.size));
    }

    // Generate MAIN Unit
    for (auto &item : mIconParamListUnitMain) {
        mIconListUnitMain.append(setColoredSvg(item.path, mColorMain, item.size));
    }

    // Generate MAIN Digit
    for (auto &item : mIconParamListDigitMain) {
        mIconListDigitMain.append(setColoredSvg(item.path, mColorMain, item.size));
    }

    // Generate AUX Symbol
    for (auto &item : mIconParamListSymAux) {
        mIconListSymAux.append(setColoredSvg(item.path, mColorAux, item.size));
    }

    // Generate AUX Unit
    for (auto &item : mIconParamListUnitAux) {
        mIconListUnitAux.append(setColoredSvg(item.path, mColorAux, item.size));
    }

    // Generate AUX Digit
    for (auto &item : mIconParamListDigitAux) {
        mIconListDigitAux.append(setColoredSvg(item.path, mColorAux, item.size));
    }

    // init dmmValue
    mDmmValue.m_integer = 0;
    mDmmValue.m_decimal = 0;
    mDmmValue.m_significant_digits = 0;
    mDmmValue.m_value = 0;
    mDmmValue.m_unit = last_unit;
    mDmmValue.m_mode = last_mode;
    mDmmValue.a_integer = 0;
    mDmmValue.a_decimal = 0;
    mDmmValue.a_significant_digits = 0;
    mDmmValue.a_value = 0;
    mDmmValue.a_unit = last_unit;
    mDmmValue.a_mode = last_mode;
    mDmmValue.tunit = last_temp_unit;
    mDmmValue.delta = last_delta_mode;
    mDmmValue.rec = last_rec_mode;
    mDmmValue.peakMode = last_peak_mode;
    mDmmValue.range = last_range_mode;
    mDmmValue.barScale = last_bar_scale;
    mDmmValue.m_overflow = false;
    mDmmValue.a_overflow = false;
    mDmmValue.low_bat = false;
    mDmmValue.visible_bar = false;
    mDmmValue.isNegative = false;
    mDmmValue.m_unitString = "";
    mDmmValue.a_unitString = mDmmValue.m_unitString;
    memset(&mDmmValue.rawData, 0, BM_DATA_SIZE);

    mDmmData = {
        .time  = QDateTime::currentDateTime(),
        .value = mDmmValue,
        .count = 0
    };

    Q_EMIT(colorChanged(DataStorage::filterColor_s({mColorMain, mColorAux})));
}

void BM86Xgui::print()
{
    QPrinter printer(QPrinter::HighResolution);

    printer.setPageSize(QPageSize::A4);
    printer.setPageMargins(QMarginsF());
    printer.setPageOrientation(QPageLayout::Landscape);

    QPrintDialog dialog(&printer, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        renderPlot(&printer);
    }
}

void BM86Xgui::renderPlot(QPaintDevice *device, const QRectF &documentRect)
{
    QwtPlotRenderer renderer;

    const bool blackAndWhite =
        ui->actionB_W_Save_Print->isChecked();

    if (blackAndWhite)
        ui->qtPlot->setBlackAndWhite();

    {
        QPainter painter(device);
        renderer.render(ui->qtPlot, &painter, documentRect.toRect());
    }

    if (blackAndWhite)
        ui->qtPlot->restoreColor();
}

void BM86Xgui::renderPlot(QPaintDevice *device)
{
    QRectF documentRect = QRectF(
    0,
    0,
    device->width(),
    device->height()
    ).adjusted(
        PlotRenderMargin,
        PlotRenderMargin,
        -PlotRenderMargin,
        -PlotRenderMargin
    );

    renderPlot(device, documentRect);
}

void BM86Xgui::savePDF(const QString &file)
{
    QString title = "BM86x : "
                    + QDate::currentDate().toString("yyyyMMdd")
                    + "_"
                    + QTime::currentTime().toString("hh-mm-ss");

    QPdfWriter pdfwriter(file);

    pdfwriter.setPageSize(QPageSize::A4);
    pdfwriter.setPageMargins(QMarginsF());
    pdfwriter.setTitle(title);
    pdfwriter.setResolution(resolution);
    pdfwriter.setPageOrientation(QPageLayout::Landscape);

    renderPlot(&pdfwriter);
}

void BM86Xgui::savePNG(const QString &file)
{
    if (file.isEmpty())
        return;

    QString extension = QFileInfo(file).suffix().toLower();

    const QRgb bg_color =
        (extension == "png")
            ? qRgba(0, 0, 0, 0)
            : qRgba(255, 255, 255, 255);

    const double mmToInch = 1.0 / 25.4;
    const int dotsPerMeter =
        qRound(resolution * mmToInch * 1000.0);

    QPageSize page_size(QPageSize::A4);
    QSizeF sizeInch = page_size.size(QPageSize::Inch);
    const QSizeF size = sizeInch * resolution;

    QImage image(
        QSize(size.height(), size.width()),
        QImage::Format_ARGB32
        );

    image.setDotsPerMeterX(dotsPerMeter);
    image.setDotsPerMeterY(dotsPerMeter);
    image.fill(bg_color);

    renderPlot(&image);

    image.save(file);
}

void BM86Xgui::saveSVG(const QString &file)
{
    QString title = "BM86x : "
                    + QDate::currentDate().toString("yyyyMMdd")
                    + "_"
                    + QTime::currentTime().toString("hh-mm-ss");

    QSvgGenerator generator;

    generator.setTitle(title);
    generator.setFileName(file);
    generator.setResolution(resolution);
    generator.setDescription(tr("BM86x"));

    QPageSize page_size(QPageSize::A4);
    QSizeF sizeInch = page_size.size(QPageSize::Inch);
    const QSizeF size = sizeInch * resolution;

    generator.setSize(QSize(size.height(), size.width()));
    generator.setViewBox(QRect(0, 0, size.height(), size.width()));

    renderPlot(&generator);
}

QPixmap BM86Xgui::setColoredSvg(const QString &svgPath,
                                const QColor &color,
                                const QSize &size)
{
    QFile file(svgPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Could not open SVG file: %s", qPrintable(svgPath));
        return QPixmap();
    }

    QString svgContent = QString::fromUtf8(file.readAll());
    file.close();

    svgContent.replace("#000000", color.name(QColor::HexRgb), Qt::CaseInsensitive);

    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QSvgRenderer renderer(svgContent.toUtf8());
    QPainter painter(&image);
    renderer.render(&painter);

    return QPixmap::fromImage(image);
}

void BM86Xgui::setColorAux(const QColor &color)
{
    if (color.isValid()) {
        mColorAux = color;

        int r, g, b;
        mColorAux.getRgb(&r, &g, &b);

        QString styleSheet =
            QString(
                "QCheckBox {"
                "    color: rgb(%1, %2, %3);"
                "}"
                "QCheckBox::indicator {"
                "    width: 12px;"
                "    height: 12px;"
                "}"
                "QCheckBox::indicator:checked {"
                "    background-color: rgba(%1, %2, %3, 255);"
                "}"
                "QCheckBox::indicator:checked:hover {"
                "    background-color: rgba(%1, %2, %3, 175);"
                "}"
                "QCheckBox::indicator:unchecked {"
                "    background-color: rgba(150, 150, 150, 255);"
                "}"
                "QCheckBox::indicator:unchecked:hover {"
                "    background-color: rgba(150, 150, 150, 175);"
                "}"
            ).arg(r).arg(g).arg(b);

        ui->cBAux->setStyleSheet(styleSheet);

        // Generate AUX Symbol
        mIconListSymAux.clear();
        for (auto &item : mIconParamListSymAux) {
            mIconListSymAux.append(setColoredSvg(item.path, mColorAux, item.size));
        }

        // Generate AUX Unit
        mIconListUnitAux.clear();
        for (auto &item : mIconParamListUnitAux) {
            mIconListUnitAux.append(setColoredSvg(item.path, mColorAux, item.size));
        }

        mIconListDigitAux.clear();
        // Generate AUX Digit
        for (auto &item : mIconParamListDigitAux) {
            mIconListDigitAux.append(setColoredSvg(item.path, mColorAux, item.size));
        }

        ui->qtPlot->setCurveColor(mColorMain, mColorAux);
        ui->qtPlot->replot();

        displayLCD(mDmmData);
        Q_EMIT(colorChanged(DataStorage::filterColor_s({mColorMain, mColorAux})));
    }
}

void BM86Xgui::setColorMain(const QColor &color)
{
    if (color.isValid()) {
        mColorMain = color;

        int r, g, b;
        mColorMain.getRgb(&r, &g, &b);
        ui->progressBar->setColor(mColorMain);

        QString styleSheet =
            QString(
                "QCheckBox {"
                "    color: rgb(%1, %2, %3);"
                "}"
                "QCheckBox::indicator {"
                "    width: 12px;"
                "    height: 12px;"
                "}"
                "QCheckBox::indicator:checked {"
                "    background-color: rgba(%1, %2, %3, 255);"
                "}"
                "QCheckBox::indicator:checked:hover {"
                "    background-color: rgba(%1, %2, %3, 175);"
                "}"
                "QCheckBox::indicator:unchecked {"
                "    background-color: rgba(150, 150, 150, 255);"
                "}"
                "QCheckBox::indicator:unchecked:hover {"
                "    background-color: rgba(150, 150, 150, 175);"
                "}"
            ).arg(r).arg(g).arg(b);

        ui->cBMain->setStyleSheet(styleSheet);

        // Generate MAIN Symbol
        mIconListSymMain.clear();
        for (auto &item : mIconParamListSymMain) {
            mIconListSymMain.append(setColoredSvg(item.path, mColorMain, item.size));
        }

        // Generate MAIN Unit
        mIconListUnitMain.clear();
        for (auto &item : mIconParamListUnitMain) {
            mIconListUnitMain.append(setColoredSvg(item.path, mColorMain, item.size));
        }

        mIconListDigitMain.clear();
        // Generate MAIN Digit
        for (auto &item : mIconParamListDigitMain) {
            mIconListDigitMain.append(setColoredSvg(item.path, mColorMain, item.size));
        }

        ui->qtPlot->setCurveColor(mColorMain, mColorAux);
        ui->qtPlot->replot();

        displayLCD(mDmmData);
        Q_EMIT(colorChanged(DataStorage::filterColor_s({mColorMain, mColorAux})));
    }
}

void BM86Xgui::setColorMousePos(const QColor &color)
{
    if (color.isValid()) {
        mColorMousePos = color;

        int r, g, b;
        mColorMousePos.getRgb(&r, &g, &b);
        ui->qtPlot->setMousePosColor(mColorMousePos);
    }
}

void BM86Xgui::setColorXAxis(const QColor &color)
{
    if (color.isValid()) {
        mColorXAxis = color;
        ui->qtPlot->setColorXAxis(color);
    }
}

void BM86Xgui::testLCD(const int &time)
{
    QList<QPixmap> list;

    // Display test
    ui->label_Mode_main->setPixmap(mIconListSymMain[BM86x::sym_main_ALL]);
    ui->label_Unit_main->setPixmap(mIconListUnitMain[BM86x::unit_main_ALL]);
    ui->lcdValue_main->setPixmap(mIconListDigitMain[BM86x::digit_main_ALL]);
    ui->lcdValue_aux->setPixmap(mIconListDigitAux[BM86x::digit_aux_ALL]);

    ui->label_Range->setPixmap(mIconListSymMain[BM86x::sym_main_Range_Auto]);

    list.append(mIconListSymMain[BM86x::sym_main_Peak_Avg]);
    list.append(mIconListSymMain[BM86x::sym_main_Peak_Max]);
    list.append(mIconListSymMain[BM86x::sym_main_Peak_Min]);
    ui->label_Peak->setPixmap(combinePixmap(list, mIconListSymMain[BM86x::sym_main_Peak_Avg].size()));

    list.clear();
    list.append(mIconListSymMain[BM86x::sym_main_Crest]);
    list.append(mIconListSymMain[BM86x::sym_main_Hold]);
    list.append(mIconListSymMain[BM86x::sym_main_Rec]);
    ui->label_Rec->setPixmap(combinePixmap(list, mIconListSymMain[BM86x::sym_main_Crest].size()));

    list.clear();
    list.append(mIconListSymAux[BM86x::sym_aux_AC]);
    list.append(mIconListSymAux[BM86x::sym_aux_minus]);
    list.append(mIconListSymAux[BM86x::sym_aux_T2]);
    list.append(mIconListSymMain[BM86x::sym_main_continuity]); // It is in the aux mode
    ui->label_Mode_aux->setPixmap(combinePixmap(list, mIconListSymAux[BM86x::sym_aux_AC].size()));
    ui->label_Unit_aux->setPixmap(mIconListUnitAux[BM86x::unit_aux_ALL]);

    ui->label_Battery->setPixmap(mIconListSymMain[BM86x::sym_main_Battery]);
    ui->label_Bar_sign->setPixmap(mIconListSymMain[BM86x::sym_main_bar_minus]);
    ui->progressBar->setHidden(false);
    ui->progressBar->setValue(ui->progressBar->maximum());

    mTestLCDTimer.start(time);
}

bool BM86Xgui::waitForHostReachable(const QString &ip)
{
    QElapsedTimer t;
    t.start();

    while (t.elapsed() < 2000)
    {
        QHostInfo info = QHostInfo::fromName(ip);
        if (info.error() == QHostInfo::NoError) {
            mTcpHostName = info.hostName();
            if (!info.addresses().isEmpty()) {
                QList<QHostAddress> ipList = info.addresses();
                QHostAddress address = ipList.first();
                // use the first IP address
                mTcpHostIP = address.toString();
            } else {
                mTcpHostIP = "";
            }

            qDebug() << info.hostName();
            qDebug() << info.addresses();
            if (mTcpHostIP != ip && mTcpHostName != ip)
            {
                mTcpHostName = "";
                mTcpHostIP = "";
                QThread::msleep(50);
                continue;
            }
            return true;
        } else {
            mTcpHostName = "";
            mTcpHostIP = "";
        }
        QThread::msleep(50);
    }
    return false;
}

void BM86Xgui::onCbAuxChecked(bool val)
{
    ui->qtPlot->onSetAuxVisible(val);
}

void BM86Xgui::onCbMainChecked(bool val)
{
    ui->qtPlot->onSetMainVisible(val);
}

void BM86Xgui::onChangePlotScale(const int &scale)
{
    ui->qtPlot->setPlotScale(scale);

    if (scale == BM86xPlot::ScaleFull) {
        ui->actionFullScale->setChecked(true);
        ui->action5_Seconds->setChecked(false);
        ui->action10_Seconds->setChecked(false);
        ui->action30_Seconds->setChecked(false);
        ui->action1_Minute->setChecked(false);
        ui->action5_Minutes->setChecked(false);
        ui->action10_Minutes->setChecked(false);
        ui->action30_Minutes->setChecked(false);
        ui->action1_hour->setChecked(false);
    }
    else if (scale == BM86xPlot::Seconds_5) {
        ui->actionFullScale->setChecked(false);
        ui->action5_Seconds->setChecked(true);
        ui->action10_Seconds->setChecked(false);
        ui->action30_Seconds->setChecked(false);
        ui->action1_Minute->setChecked(false);
        ui->action5_Minutes->setChecked(false);
        ui->action10_Minutes->setChecked(false);
        ui->action30_Minutes->setChecked(false);
        ui->action1_hour->setChecked(false);
    }
    else if (scale == BM86xPlot::Seconds_10) {
        ui->actionFullScale->setChecked(false);
        ui->action5_Seconds->setChecked(false);
        ui->action10_Seconds->setChecked(true);
        ui->action30_Seconds->setChecked(false);
        ui->action1_Minute->setChecked(false);
        ui->action5_Minutes->setChecked(false);
        ui->action10_Minutes->setChecked(false);
        ui->action30_Minutes->setChecked(false);
        ui->action1_hour->setChecked(false);
    }
    else if (scale == BM86xPlot::Seconds_30) {
        ui->actionFullScale->setChecked(false);
        ui->action5_Seconds->setChecked(false);
        ui->action10_Seconds->setChecked(false);
        ui->action30_Seconds->setChecked(true);
        ui->action1_Minute->setChecked(false);
        ui->action5_Minutes->setChecked(false);
        ui->action10_Minutes->setChecked(false);
        ui->action30_Minutes->setChecked(false);
        ui->action1_hour->setChecked(false);
    }
    else if (scale == BM86xPlot::Minutes_1) {
        ui->actionFullScale->setChecked(false);
        ui->action5_Seconds->setChecked(false);
        ui->action10_Seconds->setChecked(false);
        ui->action30_Seconds->setChecked(false);
        ui->action1_Minute->setChecked(true);
        ui->action5_Minutes->setChecked(false);
        ui->action10_Minutes->setChecked(false);
        ui->action30_Minutes->setChecked(false);
        ui->action1_hour->setChecked(false);
    }
    else if (scale == BM86xPlot::Minutes_5) {
        ui->actionFullScale->setChecked(false);
        ui->action5_Seconds->setChecked(false);
        ui->action10_Seconds->setChecked(false);
        ui->action30_Seconds->setChecked(false);
        ui->action1_Minute->setChecked(false);
        ui->action5_Minutes->setChecked(true);
        ui->action10_Minutes->setChecked(false);
        ui->action30_Minutes->setChecked(false);
        ui->action1_hour->setChecked(false);
    }
    else if (scale == BM86xPlot::Minutes_10) {
        ui->actionFullScale->setChecked(false);
        ui->action5_Seconds->setChecked(false);
        ui->action10_Seconds->setChecked(false);
        ui->action30_Seconds->setChecked(false);
        ui->action1_Minute->setChecked(false);
        ui->action5_Minutes->setChecked(false);
        ui->action10_Minutes->setChecked(true);
        ui->action30_Minutes->setChecked(false);
        ui->action1_hour->setChecked(false);
    }
    else if (scale == BM86xPlot::Minutes_30) {
        ui->actionFullScale->setChecked(false);
        ui->action5_Seconds->setChecked(false);
        ui->action10_Seconds->setChecked(false);
        ui->action30_Seconds->setChecked(false);
        ui->action1_Minute->setChecked(false);
        ui->action5_Minutes->setChecked(false);
        ui->action10_Minutes->setChecked(false);
        ui->action30_Minutes->setChecked(true);
        ui->action1_hour->setChecked(false);
    }
    else if (scale == BM86xPlot::Minutes_60) {
        ui->actionFullScale->setChecked(false);
        ui->action5_Seconds->setChecked(false);
        ui->action10_Seconds->setChecked(false);
        ui->action30_Seconds->setChecked(false);
        ui->action1_Minute->setChecked(false);
        ui->action5_Minutes->setChecked(false);
        ui->action10_Minutes->setChecked(false);
        ui->action30_Minutes->setChecked(false);
        ui->action1_hour->setChecked(true);
    }
}

void BM86Xgui::onChangePlotStyle(const int &type)
{
    ui->qtPlot->setPlotType(type);
    ui->cBplot->setCurrentIndex(type);

    if (type == BM86xPlot::PLOT_CURVE) {
        ui->actionCurve->setChecked(true);
        ui->actionLine->setChecked(false);
        ui->actionScatter->setChecked(false);
    }
    else if (type == BM86xPlot::PLOT_LINE) {
        ui->actionCurve->setChecked(false);
        ui->actionLine->setChecked(true);
        ui->actionScatter->setChecked(false);
    }
    else {
        ui->actionCurve->setChecked(false);
        ui->actionLine->setChecked(false);
        ui->actionScatter->setChecked(true);
    }
}

void BM86Xgui::onChangeReadSpeed(const int &index)
{
    if (mDeviceConnected == false) {
        return;
    }

    if (index < 0 || index >= last_speed) {
        qWarning() << "Invalid index for ReadSpeedTime.";
        return;
    }

    QByteArray pData = QByteArray::number(index);

    mTimoutTimer.stop();
    mTimoutTimer.setInterval(ReadSpeedTime[index] + 1800);
    if (mUseTcpSocket) {
        mTcpSocket.write(pData);
        mTcpSocket.waitForBytesWritten(200);
    } else {
        mSerialPort.write(pData);
        mSerialPort.waitForBytesWritten(200);
    }
    mTimoutTimer.start();
}

void BM86Xgui::onChooseColorAux()
{
    QColor color = QColorDialog::getColor(mColorAux, this, "Pick AUX color",
                                          QColorDialog::ShowAlphaChannel);
    setColorAux(color);
}

void BM86Xgui::onChooseColorMain()
{
    QColor color = QColorDialog::getColor(mColorMain, this, "Pick MAIN color",
                                          QColorDialog::ShowAlphaChannel);
    setColorMain(color);
}

void BM86Xgui::onChooseColorMousePos()
{
    QColor color = QColorDialog::getColor(mColorMousePos, this, "Pick Mouse color",
                                          QColorDialog::ShowAlphaChannel);
    setColorMousePos(color);
}

void BM86Xgui::onChooseColorXAxis()
{
    QColor color = QColorDialog::getColor(mColorXAxis, this, "Pick X Axis color",
                                          QColorDialog::ShowAlphaChannel);
    setColorXAxis(color);
}

void BM86Xgui::onConnectMultimeter(bool val)
{
    Q_UNUSED(val);
    DialogConnect dialogWindow(&mSerialPort, this);

    if (dialogWindow.exec() == QDialog::Accepted) {
        onDisconnectMultimeter(); //Just in case
        if (mSerialPort.portName() == "TCP_IP") {
            bool ok;
            QString f_IP = QInputDialog::getText(this, tr("Set TCP server IP/Name"),
                                                 tr("IP:"), QLineEdit::Normal,
                                                 mTcpQuery, &ok);

            if (!ok || f_IP.isEmpty())
                return;

            mTcpSocket.abort();
            waitForHostReachable(f_IP);
            mTcpSocket.connectToHost(mTcpQuery, BM_TCP_PORT);
            mTcpSocket.waitForConnected(500);
            mDeviceConnected = mTcpSocket.state() == QAbstractSocket::ConnectedState;
            if (mDeviceConnected) {
                mUseTcpSocket = true;
                mTcpQuery = f_IP;
            } else {
                mTcpSocket.abort();
                QThread::msleep(1000);
                waitForHostReachable(f_IP);
                mTcpSocket.connectToHost(f_IP, BM_TCP_PORT);
                mDeviceConnected = mTcpSocket.waitForConnected(500);
                if (!mDeviceConnected) {
                    return;
                }
                else {
                    mUseTcpSocket = true;
                    mTcpQuery = f_IP;
                }
            }
            QObject::connect(&mTcpSocket, &QTcpSocket::disconnected, this, &BM86Xgui::onTcpDisconnect);
        } else {
            mDeviceConnected = mSerialPort.open(QIODevice::ReadWrite);
            mUseTcpSocket = false;
            if (mDeviceConnected == false) {
                if (mSerialPort.error() != QSerialPort::NoError) {
                    qDebug() << "Serial Port error :" << mSerialPort.errorString();
                    statusBar()->showMessage(QString("Not connected UART : %1  - Error %2").
                                             arg(mSerialPort.portName(),mSerialPort.errorString()));
                    return;
                }
            }
        }

        if (mDeviceConnected == true) {

            if (mSerialPort.portName() != "TCP_IP") {
                /*
                    Open the serial port on the Arduino USB serial, reset the device.
                    So we need to wait for the device to reset, minimum 2000msec.
                    On windows, it's seem to not be needed.
                */
                if (!SERIAL_CONNECT_DELAY)
                    onConnectMultimeterSerial();
                else
                    mSerialConnectTimer.start();
            }
            else {
                QObject::connect(&mTcpSocket, &QTcpSocket::readyRead,
                                 this, &BM86Xgui::onReadSerialPort);
                QByteArray pData = "wb";
                mTcpSocket.write(pData);
                mTcpSocket.waitForBytesWritten(200);
                statusBar()->showMessage(QString("Connected TCP : %1 (%2) - Waiting data").
                                         arg(mTcpHostIP, mTcpHostName));
                onChangeReadSpeed(ui->cBSpeed->currentIndex());
                mTimoutTimer.start();
            }
        }
    }
}

void BM86Xgui::onConnectMultimeterSerial()
{
    QObject::connect(&mSerialPort, &QSerialPort::readyRead, this, &BM86Xgui::onReadSerialPort);
    QObject::connect(&mSerialPort, &QSerialPort::errorOccurred, this, &BM86Xgui::onSerialPortError);
    QByteArray pData = "wb";
    mSerialPort.write(pData);
    mSerialPort.waitForBytesWritten(200);
    statusBar()->showMessage(QString("Connected UART : %1  - Waiting data").
                             arg(mSerialPort.portName()));
    onChangeReadSpeed(ui->cBSpeed->currentIndex());
    mTimoutTimer.start();
}

void BM86Xgui::onDisconnectMultimeter(bool val)
{
    Q_UNUSED(val);
    if (mDeviceConnected == true) {
        QByteArray pData = "q";

        if (mUseTcpSocket) {
            QObject::disconnect(&mTcpSocket, &QTcpSocket::disconnected,
                                this, &BM86Xgui::onTcpDisconnect);
            QObject::disconnect(&mTcpSocket, &QTcpSocket::readyRead,
                                this, &BM86Xgui::onReadSerialPort);
            mTcpSocket.write(pData);
            mTcpSocket.waitForBytesWritten(200);
            if (mTcpSocket.isOpen()) mTcpSocket.disconnectFromHost();
            mTcpSocket.abort();
            mTcpHostName = "";
        } else {
            mSerialPort.write(pData);
            mSerialPort.waitForBytesWritten(200);
            mSerialPort.close();
            QObject::disconnect(&mSerialPort, &QSerialPort::readyRead,
                                this, &BM86Xgui::onReadSerialPort);
            QObject::disconnect(&mSerialPort, &QSerialPort::errorOccurred,
                                this, &BM86Xgui::onSerialPortError);
        }

        mDeviceConnected = false;
        mUseTcpSocket = false;
        statusBar()->showMessage("Not connected");
        mTimoutTimer.stop();
    }
}

void BM86Xgui::onDataReceived(const BM86xDataType_s &data)
{
    mTimoutTimer.start();
    if (mUseTcpSocket) {
        statusBar()->showMessage(QString("Connected TCP : %1 (%2)").arg(mTcpHostIP, mTcpHostName));
    } else {
        statusBar()->showMessage(QString("Connected UART : %1").arg(mSerialPort.portName()));
    }

    displayLCD(data);
}

void BM86Xgui::onPausePlot()
{
    mPausePlot = !mPausePlot;
    ui->qtPlot->onSetPause(mPausePlot);
    if (mPausePlot) {
        ui->pB_Pause->setIcon( setColoredSvg(":/image/Play",
                                            QColor(135, 222, 135), ui->pB_Pause->iconSize()));
    }
    else {
        ui->pB_Pause->setIcon( setColoredSvg(":/image/Pause",
                                            QColor(255, 127, 42), ui->pB_Pause->iconSize()));
        if (!mDeviceConnected) {
            ui->qtPlot->setPlotScale(currentScale);
        }
    }
}

#define _GetTick() static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch())
#define UART_DATA_TIMEOUT 20
void BM86Xgui::onReadSerialPort()
{
    static uint32_t last_call = _GetTick();
    QByteArray c_buff = mRxBuffer;
    qint8 count = 0;
    mRxBuffer.clear();
    if (mUseTcpSocket) {
        c_buff.append(mTcpSocket.readAll());
    } else {
        c_buff.append(mSerialPort.readAll());
    }

    uint32_t now = _GetTick();
    if ((c_buff.size() > BM_DATA_SIZE) && (last_call - now > UART_DATA_TIMEOUT)) {
        goto END;
    }

    if (!c_buff.isEmpty()) {
        for (QByteArray::iterator i = c_buff.begin(); i != c_buff.end(); i++) {
            count++;
            mRxBuffer.append(*i);

            if (count == BM_DATA_SIZE) {
                // qDebug() << "Received raw data:" << rxBuffer.toHex(' ').toUpper();
                mDmmValue = BM86xRawDataToVal((uint8_t*)mRxBuffer.data(), mRxBuffer.size());
                mDmmData = {
                    .time  = QDateTime::currentDateTime(),
                    .value = mDmmValue,
                    .count = std::abs( static_cast<int>(
                        mDmmValue.m_value * qPow(10, mDmmValue.m_significant_digits)
                        ) )
                };
                count = 0;
                mRxBuffer.clear();
                Q_EMIT(dataReceived(mDmmData));
            }
        }
    }

END:
    last_call = now;
    return;
}

void BM86Xgui::onRestoreLCD()
{
    displayLCD(mDmmData);
}

void BM86Xgui::onSavePlot()
{
    QString downDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

    QFileDialog dialog(this, tr("Save Plot"), downDir,
                       tr("PDF (*.pdf);;SVG (*.svg);;Image file (*.png *.jpg *.ppm *.xpm *.bmp)"));

// #ifdef Q_OS_LINUX
//     dialog.setOption(QFileDialog::DontUseNativeDialog);
// #endif

    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QList outFileList = dialog.selectedFiles();
    QString outFile = outFileList.first();

    if (outFile.isEmpty())
        return;

    const QString filter = dialog.selectedNameFilter();

    // Add the appropriate extension if none was specified.
    if (QFileInfo(outFile).suffix().isEmpty()) {
        if (filter.contains("*.pdf"))
            outFile += ".pdf";
        else if (filter.contains("*.svg"))
            outFile += ".svg";
        else
            outFile += ".png";
    }

    static QRegularExpression re_pdf("\\.pdf$", QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression re_svg("\\.svg$", QRegularExpression::CaseInsensitiveOption);

    if (outFile.contains(re_pdf)) {
        savePDF(outFile);
    }
    else if (outFile.contains(re_svg)) {
        saveSVG(outFile);
    }
    else {
        savePNG(outFile);
    }
}


void BM86Xgui::onSerialPortError(const QSerialPort::SerialPortError &error)
{
    if (error != QSerialPort::NoError) {
        if (mDeviceConnected == true) {
            // Convert the error to a string for display in the status bar
            QString errorMessage = QString("Not connected - Serial Port error : %1")
                                       .arg(mSerialPort.errorString());
            statusBar()->showMessage(errorMessage);

            mSerialPort.close();
            mDeviceConnected = false;

            mTimoutTimer.stop();
        }
    }
}

void BM86Xgui::onShowHelp()
{
    QPointer<HelpWindow> helpWin = new HelpWindow(this);

    helpWin->setAttribute(Qt::WA_DeleteOnClose);

    helpWin->show();
    helpWin->raise();
    helpWin->activateWindow();
}

void BM86Xgui::onTcpDisconnect() {
    onDisconnectMultimeter();
}

void BM86Xgui::onTestLCD()
{
    testLCD(4000);
}

void BM86Xgui::onTimeOut() {
    if (mUseTcpSocket) {
        if (mTcpSocket.state() == QAbstractSocket::ConnectedState) {
            statusBar()->showMessage(QString("Connected TCP : %1 (%2) - Device is OFF").
                                     arg(mTcpHostIP, mTcpHostName));
            return;
        }
    } else {
        mSerialPort.clear();
        if (mSerialPort.error() == QSerialPort::NoError) {
            statusBar()->showMessage(QString("Connected UART : %1  - Device is OFF").
                                     arg(mSerialPort.portName()));
            return;
        }
        else {
            mSerialPort.clearError();
        }
    }
    onDisconnectMultimeter();
}
