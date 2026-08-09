/**
 * @file datastorage.h
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#ifndef DATASTORAGE_H
#define DATASTORAGE_H

#include <QDateTime>
#include <QMainWindow>
#include <QTableWidget>
#include <QtCore/qtimer.h>
#include "bm86xdecode.h"
#include "shortcutaction.h"
#include "miniaudio.h"
#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000

#pragma pack(1)
typedef struct {
    QDateTime          time;
    BM86x_ValueTypeDef value;
} BM86xDataType_s;
#pragma pack()

namespace Ui {
class DataStorage;
}

class DataStorage : public QMainWindow
{
    Q_OBJECT

public:
#pragma pack(1)
    typedef struct {
        double min;
        double max;
    } rangeData_s;

    typedef struct {
        rangeData_s main;
        rangeData_s aux;
        bool mainActive;
        bool auxActive;
        bool inverted;
    } filterData_s;

    typedef struct {
        QColor main;
        QColor aux;
    } filterColor_s;

    typedef struct {
        bool isSoundActive;
        int  soundVolume;
        filterData_s filter;
    } config_s;

#pragma pack()

    explicit DataStorage (const QList<QPointer<QAction>> &actionList, QWidget *parent = nullptr, const bool& excludeLastRow = false);
    ~DataStorage();

    void setConfig (const config_s& config);
    config_s getConfig () const;

public Q_SLOTS:
    void onPrintData (void);
    void onExportData (void);
    void onClearData  (void);
    void onAppendData (const BM86xDataType_s &data);
    void onShowWindow (const bool &value);
    void onRecordData (const bool &value);
    void onApplyFilter (void);
    void onSetColorFilter (const filterColor_s &color);

private:
    Ui::DataStorage *ui;
    QList<BM86xDataType_s> mData;
    filterData_s           mFilterData;
    QPointer<QTableWidget> mTableWidget;
    QStringList            mModeStringList;
    QRect                  mCurrentGeometry;
    QByteArray mWindowGeometry = this->saveGeometry();
    QPointer<QWidget>      mParent;
    int  mLastRow              = 0;
    int  mColumnSize           = 100;
    bool mRecordData           = false;
    bool mFreeScroll           = false;
    bool mSoundAvailable       = false;
    bool mExcludeLastRow;

    QList<Shortcut::Action> mActionShortcutList;
    QColor mFilterColorBoth = QColor(255, 0, 0, 50);
    QColor mFilterColorMain = QColor(255, 0, 255, 50);
    QColor mFilterColorAux  = QColor(255, 255, 0, 50);
    QColor mResetColor      = QColor(0, 0, 0, 0);


    const QStringList mTableHeader = {
        "Date Time","Range","Peak","Main Mode","Main Unit",
        "Main Value","Aux Mode","Aux Unit","Aux Value"
    };

    QPixmap setColoredSvg (const QString& svgPath, const QColor& color, const QSize& size);
    void playSound ();
    void copySelectionToClipboard();
    QColor mixColors(const QColor &c1, const QColor &c2);

    // Minisound
    QTimer mSoundTimer;
    ma_waveform        mSineWave;
    ma_device_config   mSoundDeviceConfig;
    ma_device          mSoundDevice;
    ma_waveform_config mSineWaveConfig;

private Q_SLOTS:
    void onVolumeChanged (int value);
    void onStopSound (void);
    void onScrollLock (void);

Q_SIGNALS:
    void showStatusCanged (const bool &value);
    void recordDataChanged (const bool &value);
    void filterDataChanged (const filterData_s &value);

protected:
    void closeEvent (QCloseEvent *event) override;
    void hideEvent (QHideEvent *event) override;
    void showEvent (QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

#endif // DATASTORAGE_H
