/**
 * @file datastorage.cpp
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#include "datastorage.h"
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTextStream>
#include <QtGui/qevent.h>
#include <QtGui/qscreen.h>
#include <QtGui/qtextdocument.h>
#include <QClipboard>
#include "ui_datastorage.h"
#include "xlsxconditionalformatting.h"
#include "xlsxdocument.h"

#define SOUND_DURATION  100  // in milliseconds
#define SOUND_FREQUENCY 2800 // Sine frequency in Hz
void soundData_callback
(
    ma_device *pDevice,
    void *pOutput,
    const void *pInput,
    ma_uint32 frameCount
)
{
    ma_waveform_read_pcm_frames((ma_waveform*)pDevice->pUserData, pOutput, frameCount, NULL);
    (void)pInput;   /* Unused. */
}

constexpr qsizetype MaxXlsxRows = 1'048'576;

DataStorage::DataStorage(const QList<QPointer<QAction>> &actionList, QWidget *parent, const bool &excludeLastRow)
    : QMainWindow(parent), ui(new Ui::DataStorage)
    , mExcludeLastRow(excludeLastRow)
{
    ui->setupUi(this);
    mParent = parent;
    setWindowTitle(QString("%1 - Data").arg(_TARGET));

    mModeStringList = {
        "VDC",
        "VAC",
        "VADC",
        "IDC",
        "IAC",
        "IADC",
        "Freq",
        "Duty",
        "Res",
        "Cap",
        "Diode",
        "Cont",
        "VFD",
        "VFDHz",
        "dBm",
        "%4~20mA",
        "T1",
        "T2",
        "T1-T2",
        ""
    };   

    QObject::connect(ui->cB_Filter_main, &QCheckBox::clicked, this, &DataStorage::onApplyFilter);
    QObject::connect(ui->cB_Filter_aux, &QCheckBox::clicked, this, &DataStorage::onApplyFilter);
    QObject::connect(ui->cBInvertFilter, &QCheckBox::clicked, this, &DataStorage::onApplyFilter);
    QObject::connect(ui->dSB_main_max, &QDoubleSpinBox::valueChanged, this, &DataStorage::onApplyFilter);
    QObject::connect(ui->dSB_main_min, &QDoubleSpinBox::valueChanged, this, &DataStorage::onApplyFilter);
    QObject::connect(ui->dSB_aux_max, &QDoubleSpinBox::valueChanged, this, &DataStorage::onApplyFilter);
    QObject::connect(ui->dSB_aux_min, &QDoubleSpinBox::valueChanged, this, &DataStorage::onApplyFilter);

    QObject::connect(ui->pB_close, &QPushButton::clicked, this, &QMainWindow::close);
    QObject::connect(ui->pB_clear, &QPushButton::clicked, this, &DataStorage::onClearData);
    QObject::connect(ui->pB_export, &QPushButton::clicked, this, &DataStorage::onExportData);
    QObject::connect(ui->pB_Record, &QPushButton::clicked, this, [=, this] () {
        mRecordData = !mRecordData;
        if (mRecordData) {
            ui->pB_Record->setIcon(setColoredSvg(":/image/Record_Data_Stop", QColor(229, 128, 255), ui->pB_Record->iconSize()));
        }
        else {
            ui->pB_Record->setIcon(setColoredSvg(":/image/Record_Data", QColor(229, 128, 255), ui->pB_Record->iconSize()));
        }
        Q_EMIT recordDataChanged(mRecordData);
    });

    QObject::connect(ui->pB_lockScroll, &QPushButton::clicked, this, &DataStorage::onScrollLock);

    auto actionClose = new QAction(this);
    addAction(actionClose);
    QObject::connect(actionClose, &QAction::triggered, this, &QMainWindow::close);

    // Set ShortcutContext
    actionClose->setShortcutContext(Qt::WindowShortcut); // By default it's WindowShortcut, but to be clear we set it here

    /* {QString(text),QString(toolTip),QAction(*action),QKeySequence(shortcut)} */
    mActionShortcutList.append({
        "Close",
        "Close window",
        actionClose,
        // Qt::ControlModifier | Qt::Key_W
        QKeySequence::Close
    });

    auto actionScrollLock = new QAction(this);
    addAction(actionScrollLock);
    QObject::connect(actionScrollLock, &QAction::triggered, this, &DataStorage::onScrollLock);

    // Set ShortcutContext
    actionScrollLock->setShortcutContext(Qt::WindowShortcut);

    mActionShortcutList.append({
        "Lock",
        "Lock scroll lock",
        actionScrollLock,
        Qt::ControlModifier | Qt::Key_L
    });

    for (auto &item : mActionShortcutList)
        item.apply();

    // Check actionList size
    if (actionList.size() > 2) {
        QList<Shortcut::ButtonParam> buttonList;
        buttonList.append({ui->pB_close, actionClose});
        buttonList.append({ui->pB_lockScroll, actionScrollLock});
        buttonList.append({ui->pB_export, actionList.at(0)});
        buttonList.append({ui->pB_clear, actionList.at(1)});
        buttonList.append({ui->pB_Record, actionList.at(2)});

        for (auto &item : buttonList) {
            QString currentTooltip = item.button->toolTip();
            QString currentActionShortcut = item.action->shortcut().toString(QKeySequence::NativeText);
            QString finalToolTip = QString("%1\n%2").arg(currentTooltip, currentActionShortcut);
            item.button->setToolTip(finalToolTip);
        }
    }

    // Set table property
    mTableWidget = ui->tableWidget;
    mTableWidget->setColumnCount(mTableHeader.size());
    mTableWidget->setHorizontalHeaderLabels(mTableHeader);
    mTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int i = 0; i < mTableHeader.size(); ++i) {
        if (i == mTableHeader.indexOf("Date Time"))
            mTableWidget->setColumnWidth(i,mColumnSize * 2);
        else
            mTableWidget->setColumnWidth(i,mColumnSize);
    }

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

    ui->pB_export->setText("");
    ui->pB_clear->setText("");
    ui->pB_close->setText("");
    ui->pB_Record->setText("");
    ui->pB_lockScroll->setText("");
    ui->pB_export->setIcon(setColoredSvg(":/image/Save_Data",QColor(85, 153, 255), ui->pB_export->iconSize()));
    ui->pB_clear->setIcon(setColoredSvg(":/image/Clear_Data", QColor(85, 153, 255), ui->pB_clear->iconSize()));
    ui->pB_close->setIcon(setColoredSvg(":/image/Close", QColor(255, 0, 0), ui->pB_close->iconSize()));
    ui->pB_Record->setIcon(setColoredSvg(":/image/Record_Data", QColor(229, 128, 255), ui->pB_Record->iconSize()));
    ui->pB_lockScroll->setIcon(setColoredSvg(":/image/Scroll_OFF", QColor(255, 127, 42), ui->pB_Record->iconSize()));
    ui->pB_export->setStyleSheet(buttonStyleSheet);
    ui->pB_clear->setStyleSheet(buttonStyleSheet);
    ui->pB_close->setStyleSheet(buttonStyleSheet);
    ui->pB_Record->setStyleSheet(buttonStyleSheet);
    ui->pB_lockScroll->setStyleSheet(buttonStyleSheet);

    // Miniaudio
    mSoundDeviceConfig = ma_device_config_init(ma_device_type_playback);
    mSoundDeviceConfig.playback.format   = DEVICE_FORMAT;
    mSoundDeviceConfig.playback.channels = DEVICE_CHANNELS;
    mSoundDeviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    mSoundDeviceConfig.dataCallback      = soundData_callback;
    mSoundDeviceConfig.pUserData         = &mSineWave;

    if (ma_device_init(NULL, &mSoundDeviceConfig, &mSoundDevice) != MA_SUCCESS) {
        qDebug() << "Failed to open playback device.\n";
    }
    else {
        mSoundAvailable = true;
        mSineWaveConfig = ma_waveform_config_init(
            mSoundDevice.playback.format, mSoundDevice.playback.channels,
            mSoundDevice.sampleRate, ma_waveform_type_sine,
            (double)ui->dial_volume->value() / 20.0, SOUND_FREQUENCY
        );
        ma_waveform_init(&mSineWaveConfig, &mSineWave);

        mSoundTimer.setInterval(SOUND_DURATION);
        mSoundTimer.setSingleShot(true);
        QObject::connect(ui->dial_volume, &QDial::valueChanged, this, &DataStorage::onVolumeChanged);
        QObject::connect(&mSoundTimer, &QTimer::timeout, this, &DataStorage::onStopSound);
    }

    if (mSoundAvailable == false) {
        ui->dial_volume->setHidden(true);
        ui->cB_sound->setHidden(true);
    }

    // Init Filter for QwPlot, and ...
    mFilterData.mainActive = ui->cB_Filter_main->isChecked();
    mFilterData.auxActive  = ui->cB_Filter_aux->isChecked();
    mFilterData.inverted   = ui->cBInvertFilter->isChecked();
    mFilterData.main       = {ui->dSB_main_min->value(),ui->dSB_main_max->value()};
    mFilterData.aux        = {ui->dSB_aux_min->value(),ui->dSB_aux_max->value()};

    Q_EMIT(filterDataChanged(mFilterData));
    mWindowGeometry = this->saveGeometry();
}

DataStorage::~DataStorage()
{
    ma_device_uninit(&mSoundDevice);
    ma_waveform_uninit(&mSineWave);
    delete ui;
}

void DataStorage::setConfig(const config_s &config)
{
    ui->cB_sound->setChecked(config.isSoundActive);
    ui->dial_volume->setValue(config.soundVolume);
    ui->cB_Filter_main->setChecked(config.filter.mainActive);
    ui->cB_Filter_aux->setChecked(config.filter.auxActive);
    ui->cBInvertFilter->setChecked(config.filter.inverted);
    ui->dSB_main_max->setValue(config.filter.main.max);
    ui->dSB_main_min->setValue(config.filter.main.min);
    ui->dSB_aux_max->setValue(config.filter.aux.max);
    ui->dSB_aux_min->setValue(config.filter.aux.min);
}

DataStorage::config_s DataStorage::getConfig() const
{
    config_s config;
    config.isSoundActive = ui->cB_sound->isChecked();
    config.soundVolume = ui->dial_volume->value();
    config.filter.mainActive = ui->cB_Filter_main->isChecked();
    config.filter.auxActive = ui->cB_Filter_aux->isChecked();
    config.filter.inverted = ui->cBInvertFilter->isChecked();
    config.filter.main.max = ui->dSB_main_max->value();
    config.filter.main.min = ui->dSB_main_min->value();
    config.filter.aux.max = ui->dSB_aux_max->value();
    config.filter.aux.min = ui->dSB_aux_min->value();

    return config;
}

void DataStorage::onPrintData(void)
{
    QPrinter printer(QPrinter::PrinterResolution);

    printer.setPageSize(QPageSize::A4);
    printer.setPageMargins(QMarginsF());
    printer.setPageOrientation(QPageLayout::Portrait);

    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString html;
    html.reserve(50000);

    html += "<h2>" + QString(_TARGET) + " Measurements</h2>";
    html += "<p>";
    html += QDateTime::currentDateTime().toString();
    html += "</p>";

    html += R"(
        <style type="text/css">

        table
        {
            display: block;
            overflow-y: auto;
            overflow-x: auto;
            padding-right: 5pt;
            border-collapse: collapse;
            font-family : "Menlo", "Consolas", "Courier New";
            font-size: 8pt;
        }

        th
        {
            background-color: #D9D9D9;
        }
        td, th {
           text-align: center;
           vertical-align: middle;
           padding-left: 5pt;
           padding-right: 5pt;
        }
    )";

    int r, g, b;
    mFilterColorBoth.getRgb(&r, &g, &b);
    html += QString(
        "tr.both td {"
        "    background-color: rgb(%1, %2, %3);"
        "}"
    ).arg(r).arg(g).arg(b);

    mFilterColorMain.getRgb(&r, &g, &b);
    html += QString(
        "tr.main td {"
        "    background-color: rgb(%1, %2, %3);"
        "}"
    ).arg(r).arg(g).arg(b);

    mFilterColorAux.getRgb(&r, &g, &b);
    html += QString(
        "tr.aux td {"
        "    background-color: rgb(%1, %2, %3);"
        "}"
        ""
        "</style>"
    ).arg(r).arg(g).arg(b);

    html += "<table border='1' text-align=center>";

    html += "<tr>";

    for (int c = 0; c < mTableHeader.length(); c++)
    {
        html += "<th>";
        html += mTableHeader.at(c).toHtmlEscaped();
        html += "</th>";
    }

    html += "</tr>";

    qsizetype maxVal;
    if (mRecordData && mExcludeLastRow)
        maxVal = mTableWidget->rowCount() - 1; // We remove last value
    else
        maxVal = mTableWidget->rowCount();

    const int mainCol = mTableHeader.indexOf("Main Value");
    const int auxCol  = mTableHeader.indexOf("Aux Value");

    bool okMain;
    bool okAux;
    double exportMainValue = 0.0f;
    double exportAuxValue = 0.0f;

    for (int row = 0; row < maxVal; row++)
    {
        double main_value = 0.0f;
        double aux_value  = 0.0f;

        exportMainValue =  QLocale().toDouble(mTableWidget->item(row, mainCol)->text(), &okMain);
        exportAuxValue =  QLocale().toDouble(mTableWidget->item(row, auxCol)->text(), &okAux);

        if (okMain) main_value = exportMainValue;
        if (okAux) aux_value = exportAuxValue;
        bool aux_isNotEmpty = mTableWidget->item(row, auxCol)->text() == "" ? false : true;

        auto *mainItem = mTableWidget->item(row, mainCol);
        auto *auxItem  = mTableWidget->item(row, auxCol);
        QString rowClass;

        if (ui->cBInvertFilter->isChecked()) {
            if (ui->cB_Filter_main->isChecked() && ui->cB_Filter_aux->isChecked()) {
                if ( (main_value >= ui->dSB_main_min->value() && main_value <= ui->dSB_main_max->value())
                    && aux_isNotEmpty && (aux_value >= ui->dSB_aux_min->value() && aux_value <= ui->dSB_aux_max->value()) ) {
                    rowClass = "both";
                }
                else if ( main_value >= ui->dSB_main_min->value() && main_value <= ui->dSB_main_max->value() ) {
                    rowClass = "main";
                }
                else if ( aux_isNotEmpty && aux_value >= ui->dSB_aux_min->value() && aux_value <= ui->dSB_aux_max->value() ) {
                    rowClass = "aux";
                }
            }
            else if (ui->cB_Filter_main->isChecked()) {
                if ( main_value >= ui->dSB_main_min->value() && main_value <= ui->dSB_main_max->value() ) {
                    rowClass = "main";
                }
            }
            else if (ui->cB_Filter_aux->isChecked()) {
                if ( aux_isNotEmpty && aux_value >= ui->dSB_aux_min->value() && aux_value <= ui->dSB_aux_max->value() ) {
                    rowClass = "aux";
                }
            }
        }
        else {
            if (ui->cB_Filter_main->isChecked() && ui->cB_Filter_aux->isChecked() ) {
                if ( mainItem->text() == "OL." && auxItem->text() == "OL.") {
                    rowClass = "both";
                }
                else if ( mainItem->text() == "OL.") {
                    rowClass = "main";
                }
                else if ( auxItem->text() == "OL.") {
                    rowClass = "aux";
                }
                else if ( (main_value < ui->dSB_main_min->value() || main_value > ui->dSB_main_max->value() )
                           && aux_isNotEmpty && (aux_value < ui->dSB_aux_min->value() || aux_value > ui->dSB_aux_max->value()) ) {
                    rowClass = "both";
                }
                else if ( main_value < ui->dSB_main_min->value() || main_value > ui->dSB_main_max->value() ) {
                    rowClass = "main";
                }
                else if ( aux_isNotEmpty && (aux_value < ui->dSB_aux_min->value() || aux_value > ui->dSB_aux_max->value()) ) {
                    rowClass = "aux";
                }
            }
            else if (ui->cB_Filter_main->isChecked()) {
                if ( mainItem->text() == "OL." ) {
                    rowClass = "main";
                }
                else if ( main_value < ui->dSB_main_min->value() || main_value > ui->dSB_main_max->value() ) {
                    rowClass = "main";
                }
            }
            else if (ui->cB_Filter_aux->isChecked()) {
                if ( auxItem->text() == "OL." ) {
                    rowClass = "aux";
                }
                else if ( aux_isNotEmpty && (aux_value < ui->dSB_aux_min->value() || aux_value > ui->dSB_aux_max->value()) ) {
                    rowClass = "aux";
                }
            }
        }

        if (rowClass.isEmpty())
            html += "<tr>";
        else
            html += "<tr class=\"" + rowClass + "\">";

        for (int col = 0; col < mTableWidget->columnCount(); col++)
        {
            html += "<td>";
            html += mTableWidget->item(row, col)->text().toHtmlEscaped();
            html += "</td>";
        }

        html += "</tr>";
    }

    html += "</table>";

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&printer);
}

void DataStorage::onExportData()
{
    const int mainCol = mTableHeader.indexOf("Main Value");
    const int auxCol  = mTableHeader.indexOf("Aux Value");
    const int columnCount = mTableWidget->columnCount();

    bool ok;
    double exportValue = 0.0f;
    QString outFile;
    QString downDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

    QFileDialog dialog(this, tr("Save File"), downDir);

// #ifdef Q_OS_LINUX
//     dialog.setOption(QFileDialog::DontUseNativeDialog);
// #endif

    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (mTableWidget->rowCount() + 1 > MaxXlsxRows) {
        dialog.setNameFilter(tr("CSV (*.csv)"));
    }
    else {
        dialog.setNameFilter(tr("Excel (*.xlsx);;CSV (*.csv)"));
    }

    if (dialog.exec() == QDialog::Accepted) {
        QList outFileList = dialog.selectedFiles();
        outFile = outFileList.first();

        if (!outFile.isEmpty() && QFileInfo(outFile).suffix().isEmpty()) {
            const QString filter = dialog.selectedNameFilter();

            if (filter.startsWith("Excel"))
                outFile += ".xlsx";
            else if (filter.startsWith("CSV"))
                outFile += ".csv";
        }
    }


    if (outFile.isEmpty())
        return;

    if (outFile.endsWith(".xlsx", Qt::CaseInsensitive) && mTableWidget->rowCount() + 1 > MaxXlsxRows)
    {
        QMessageBox::warning(
            this,
            tr("Export error"),
            tr("The table contains %1 rows.\n\n"
               "An Excel (.xlsx) worksheet is limited to %2 rows.\n"
               "Please export the data as CSV instead.")
                .arg(mTableWidget->rowCount() + 1)
                .arg(MaxXlsxRows));

        return;
    }

    qsizetype maxVal;
    if (mRecordData && mExcludeLastRow)
        maxVal = mTableWidget->rowCount() - 1; // We remove last value
    else
        maxVal = mTableWidget->rowCount();

    if (outFile.endsWith(".xlsx", Qt::CaseInsensitive) && mTableWidget->rowCount() + 1 <= MaxXlsxRows) {
        QXlsx::Document xlsxW;

        // Apply filter
        QXlsx::Format format_both, format_main, format_aux;
        format_both.setPatternBackgroundColor(mFilterColorBoth.toRgb());
        format_main.setPatternBackgroundColor(mFilterColorMain.toRgb());
        format_aux.setPatternBackgroundColor(mFilterColorAux.toRgb());

        QString conditionalFormula_both, conditionalFormula_main, conditionalFormula_aux;
        QXlsx::ConditionalFormatting cf_both, cf_main, cf_aux;

        if (ui->cBInvertFilter->isChecked()) {
            if (ui->cB_Filter_main->isChecked() && ui->cB_Filter_aux->isChecked()) {
                // BOTH
                conditionalFormula_both =
                    QString("AND( AND($F2<=%1,$F2>=%2),AND($I2<=%3,$I2>=%4) )")
                        .arg(ui->dSB_main_max->value())
                        .arg(ui->dSB_main_min->value())
                        .arg(ui->dSB_aux_max->value())
                        .arg(ui->dSB_aux_min->value())
                    ;

                cf_both.addHighlightCellsRule(QXlsx::ConditionalFormatting::Highlight_Expression,conditionalFormula_both, format_both);
                cf_both.addRange(2, 1, maxVal + 1, columnCount);

                xlsxW.addConditionalFormatting(cf_both);

                // MAIN
                conditionalFormula_main =
                    QString("AND( AND($F2<=%1,$F2>=%2),NOT(AND($I2<=%3,$I2>=%4)) )")
                        .arg(ui->dSB_main_max->value())
                        .arg(ui->dSB_main_min->value())
                        .arg(ui->dSB_aux_max->value())
                        .arg(ui->dSB_aux_min->value())
                    ;

                cf_main.addHighlightCellsRule(QXlsx::ConditionalFormatting::Highlight_Expression,conditionalFormula_main, format_main);
                cf_main.addRange(2, 1, maxVal + 1, columnCount);

                xlsxW.addConditionalFormatting(cf_main);

                // AUX
                conditionalFormula_aux =
                    QString("AND( NOT(AND($F2<=%1,$F2>=%2)),AND($I2<=%3,$I2>=%4) )")
                        .arg(ui->dSB_main_max->value())
                        .arg(ui->dSB_main_min->value())
                        .arg(ui->dSB_aux_max->value())
                        .arg(ui->dSB_aux_min->value())
                    ;

                cf_aux.addHighlightCellsRule(QXlsx::ConditionalFormatting::Highlight_Expression,conditionalFormula_aux, format_aux);
                cf_aux.addRange(2, 1, maxVal + 1, columnCount);

                xlsxW.addConditionalFormatting(cf_aux);
            }
            else if (ui->cB_Filter_main->isChecked()) {
                // MAIN
                conditionalFormula_main =
                    QString("AND($F2<=%1,$F2>=%2)")
                        .arg(ui->dSB_main_max->value())
                        .arg(ui->dSB_main_min->value())
                    ;

                cf_main.addHighlightCellsRule(QXlsx::ConditionalFormatting::Highlight_Expression,conditionalFormula_main, format_main);
                cf_main.addRange(2, 1, maxVal + 1, columnCount);

                xlsxW.addConditionalFormatting(cf_main);
            }
            else if (ui->cB_Filter_aux->isChecked()) {
                // AUX
                conditionalFormula_aux =
                    QString("AND($I2<=%1,$I2>=%2)")
                        .arg(ui->dSB_aux_max->value())
                        .arg(ui->dSB_aux_min->value())
                    ;

                cf_aux.addHighlightCellsRule(QXlsx::ConditionalFormatting::Highlight_Expression,conditionalFormula_aux, format_aux);
                cf_aux.addRange(2, 1, maxVal + 1, columnCount);

                xlsxW.addConditionalFormatting(cf_aux);
            }
        }
        else {
            if (ui->cB_Filter_main->isChecked() && ui->cB_Filter_aux->isChecked()) {
                // BOTH
                conditionalFormula_both = QString(R"(OR(  AND($F2="OL.",$I2="OL."))");
                conditionalFormula_both.append(
                    QString(",AND( OR($F2>%1,$F2<%2),OR($I2>%3,$I2<%4) )  )")
                        .arg(ui->dSB_main_max->value())
                        .arg(ui->dSB_main_min->value())
                        .arg(ui->dSB_aux_max->value())
                        .arg(ui->dSB_aux_min->value())
                    );

                cf_both.addHighlightCellsRule(QXlsx::ConditionalFormatting::Highlight_Expression,conditionalFormula_both, format_both);
                cf_both.addRange(2, 1, maxVal + 1, columnCount);

                xlsxW.addConditionalFormatting(cf_both);

                // MAIN
                conditionalFormula_main = QString(R"(OR(  AND($F2="OL.",NOT($I2="OL.")))");
                conditionalFormula_main.append(
                    QString(",AND( OR($F2>%1,$F2<%2),NOT(OR($I2>%3,$I2<%4)) )  )")
                        .arg(ui->dSB_main_max->value())
                        .arg(ui->dSB_main_min->value())
                        .arg(ui->dSB_aux_max->value())
                        .arg(ui->dSB_aux_min->value())
                    );

                cf_main.addHighlightCellsRule(QXlsx::ConditionalFormatting::Highlight_Expression,conditionalFormula_main, format_main);
                cf_main.addRange(2, 1, maxVal + 1, columnCount);

                xlsxW.addConditionalFormatting(cf_main);

                // AUX
                conditionalFormula_aux = QString(R"(OR(  AND(NOT($F2="OL."),$I2="OL."))");
                conditionalFormula_aux.append(
                    QString(",AND( NOT(OR($F2>%1,$F2<%2)),OR($I2>%3,$I2<%4) )  )")
                        .arg(ui->dSB_main_max->value())
                        .arg(ui->dSB_main_min->value())
                        .arg(ui->dSB_aux_max->value())
                        .arg(ui->dSB_aux_min->value())
                    );

                cf_aux.addHighlightCellsRule(QXlsx::ConditionalFormatting::Highlight_Expression,conditionalFormula_aux, format_aux);
                cf_aux.addRange(2, 1, maxVal + 1, columnCount);

                xlsxW.addConditionalFormatting(cf_aux);
            }
            else if (ui->cB_Filter_main->isChecked()) {
                // MAIN
                conditionalFormula_main = QString(R"(OR( $F2="OL.")");
                conditionalFormula_main.append(
                    QString(",OR($F2>%1,$F2<%2) )")
                        .arg(ui->dSB_main_max->value())
                        .arg(ui->dSB_main_min->value())
                    );

                cf_main.addHighlightCellsRule(QXlsx::ConditionalFormatting::Highlight_Expression,conditionalFormula_main, format_main);
                cf_main.addRange(2, 1, maxVal + 1, columnCount);

                xlsxW.addConditionalFormatting(cf_main);
            }
            else if (ui->cB_Filter_aux->isChecked()) {
                // AUX
                conditionalFormula_aux = QString(R"(OR( $I2="OL.")");
                conditionalFormula_aux.append(
                    QString(",OR($I2>%1,$I2<%2) )")
                        .arg(ui->dSB_aux_max->value())
                        .arg(ui->dSB_aux_min->value())
                    );

                cf_aux.addHighlightCellsRule(QXlsx::ConditionalFormatting::Highlight_Expression,conditionalFormula_aux, format_aux);
                cf_aux.addRange(2, 1, maxVal + 1, columnCount);

                xlsxW.addConditionalFormatting(cf_aux);
            }
        }

        for (int col = 0; col < columnCount; col++)
        {
            QTableWidgetItem *headerItem = mTableWidget->horizontalHeaderItem(col);
            if (headerItem) {
                xlsxW.write(1, col + 1, headerItem->text());
            }
        }

        for (int row=0; row < maxVal; row++)
        {
            for (int col=0; col < columnCount; col++)
            {
                QTableWidgetItem *item = mTableWidget->item(row, col);
                if (item) {
                    if (col == mainCol || col == auxCol) {
                        exportValue = QLocale().toDouble(item->text(), &ok);
                        if (ok) xlsxW.write(row + 2, col + 1, exportValue);
                    }
                    else {
                        xlsxW.write(row + 2, col + 1, item->text());
                    }
                }
            }
        }

        // Format Table
        QXlsx::Format formatColumn;
        formatColumn.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
        formatColumn.setVerticalAlignment(QXlsx::Format::AlignVCenter);
        xlsxW.setColumnFormat(1, columnCount, formatColumn);
        xlsxW.autosizeColumnWidth(1, columnCount);

        xlsxW.setDocumentProperty("title",QString(QString(_TARGET) + " Measurement"));
        xlsxW.setDocumentProperty("description",QString("Measurement : %1").arg(QDateTime::currentDateTime().toString()));
        xlsxW.setDocumentProperty("keywords",QString("DMM"));

        xlsxW.saveAs(outFile);
    }
    else {
        QFile file(outFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8); // Qt6
        file.write("\xEF\xBB\xBF"); // BOM UTF-8
        QString sep = QLocale().decimalPoint() == ',' ? ";" : ",";

        int columnCount = mTableWidget->columnCount();
        // Write header
        for (int col = 0; col < columnCount; ++col)
        {
            QTableWidgetItem *headerItem = mTableWidget->horizontalHeaderItem(col);
            if (headerItem) {
                out << headerItem->text();
                if (col < columnCount - 1) out << sep;
            }
        }
        out << "\n";


        // Write data
        for (int row = 0; row < maxVal; ++row)
        {
            for (int col = 0; col < columnCount; ++col)
            {
                QTableWidgetItem *item = mTableWidget->item(row, col);
                if (item) {
                    if (col == mainCol || col == auxCol) {
                        exportValue = QLocale().toDouble(item->text(), &ok);
                        if (ok) out << exportValue;
                    }
                    else {
                        out << item->text();
                    }

                    if (col < columnCount - 1) out << sep;
                }
            }
            out << "\n";
        }
    }
}

void DataStorage::onClearData()
{
    mTableWidget->clearContents();
    mTableWidget->setRowCount(0);
    mData.clear();
    mLastRow = 0;
}

void DataStorage::onAppendData(const BM86xDataType_s &data)
{
    int row = mTableWidget->rowCount();

    if (!mRecordData) {
        row = mLastRow;
        mTableWidget->removeRow(row);
    }
    else {
        mData.append(data);
    }

    mTableWidget->insertRow(row);

    QString time = data.time.toString("dd/MM/yyyy hh:mm:ss.zzz");
    QString range, peak, m_mode, m_unit, m_value, a_mode, a_unit, a_value;

    if (data.value.range == Auto)
        range = "Auto";

    // Set Main Mode
    if (data.value.m_mode < last_mode)
        m_mode = mModeStringList[data.value.m_mode];
    else
        m_mode = "";

    // Set Aux Mode
    if (data.value.a_mode < last_mode)
        a_mode = mModeStringList[data.value.a_mode];
    else
        a_mode = "";

    //Set Main Unit
    if (data.value.m_mode == Res || data.value.m_mode == Cont) {
        if (data.value.m_unit == ohm_unit) {
            m_unit ="\u03A9";
        }
        else if (data.value.m_unit == Kohm_unit) {
            m_unit ="k\u03A9";
        }
        else {
            m_unit ="M\u03A9";
        }
    }
    else {
        m_unit = data.value.m_unitString;
    }

    //Set Aux Unit
    if (data.value.a_mode == Res || data.value.a_mode == Cont) {
        if (data.value.a_unit == ohm_unit) {
            a_unit ="\u03A9";
        }
        else if (data.value.a_unit == Kohm_unit) {
            a_unit ="k\u03A9";
        }
        else {
            a_unit ="M\u03A9";
        }
    }
    else {
        a_unit = data.value.a_unitString;
    }

    //Set Rec
    if (data.value.rec == HoldRec)
        peak += "H R ";
    else if (data.value.rec == Rec)
        peak += "R ";
    else if (data.value.rec == HoldCrest)
        peak += "H C ";
    else if (data.value.rec == Crest)
        peak += "C ";

    //Set Peak
    if (data.value.peakMode == Min)
        peak += "Min";
    else if (data.value.peakMode == Max)
        peak += "Max";
    else if (data.value.peakMode == Avg)
        peak += "Avg";
    else if (data.value.peakMode == All)
        peak += "Max-Min-Avg";
    else
        peak += "";

    if (data.value.m_overflow == true) {
        m_value = "OL.";
    }
    else {
        QLocale locale;
        m_value = locale.toString(data.value.m_value, 'f', data.value.m_significant_digits);
    }

    if (data.value.a_overflow == true) {
        a_value = "OL.";
    }
    else {
        QLocale locale;
        if (data.value.a_mode < last_mode)
            a_value = locale.toString(data.value.a_value, 'f', data.value.a_significant_digits);
    }

    QMap<QString, QString> values = {
        {"Date Time", time},
        {"Range", range},
        {"Peak", peak},
        {"Main Mode", m_mode},
        {"Main Unit", m_unit},
        {"Main Value", m_value},
        {"Aux Mode", a_mode},
        {"Aux Unit", a_unit},
        {"Aux Value", a_value}
    };

    for (int i = 0; i < mTableHeader.size(); ++i) {
        const QString &header = mTableHeader.at(i);

        if (values.contains(header))
            mTableWidget->setItem(row, i, new QTableWidgetItem(values.value(header)));
    }

    bool catched = false;
    const int mainCol = mTableHeader.indexOf("Main Value");
    const int auxCol  = mTableHeader.indexOf("Aux Value");
    double main_value = (double)data.value.m_value;
    double aux_value = (double)data.value.a_value;
    bool aux_isNotEmpty = mTableWidget->item(row, auxCol)->text() == "" ? false : true;

    QBrush curColor;

    //Check Filter
    if (ui->cBInvertFilter->isChecked()) {
        if (ui->cB_Filter_main->isChecked() && ui->cB_Filter_aux->isChecked()) {
            if ( (main_value >= ui->dSB_main_min->value() && main_value <= ui->dSB_main_max->value())
                && (aux_isNotEmpty && (aux_value >= ui->dSB_aux_min->value() && aux_value <= ui->dSB_aux_max->value()) ) )
            {
                curColor = QBrush(mFilterColorBoth);
                catched = true;
            }
            else if ( main_value >= ui->dSB_main_min->value() && main_value <= ui->dSB_main_max->value() ) {
                curColor = QBrush(mFilterColorMain);
                catched = true;
            }
            else if ( aux_isNotEmpty && aux_value >= ui->dSB_aux_min->value() && aux_value <= ui->dSB_aux_max->value() ) {
                curColor = QBrush(mFilterColorAux);
                catched = true;
            }
        }
        else if (ui->cB_Filter_main->isChecked()) {
            if ( main_value >= ui->dSB_main_min->value() && main_value <= ui->dSB_main_max->value() ) {
                curColor = QBrush(mFilterColorMain);
                catched = true;
            }
            else {
                curColor = QBrush(mResetColor);
            }
        }
        else if (ui->cB_Filter_aux->isChecked()) {
            if ( aux_isNotEmpty && aux_value >= ui->dSB_aux_min->value() && aux_value <= ui->dSB_aux_max->value() ) {
                curColor = QBrush(mFilterColorAux);
                catched = true;
            }
            else {
                curColor = QBrush(mResetColor);
            }
        }
        else {
            curColor = QBrush(mResetColor);
        }
    }
    else {
        if (ui->cB_Filter_main->isChecked() && ui->cB_Filter_aux->isChecked()) {
            if ( mTableWidget->item(row, mainCol)->text() == "OL."
                && mTableWidget->item(row, auxCol)->text() == "OL." ) {
                curColor = QBrush(mFilterColorBoth);
                catched = true;
            }
            else if ( mTableWidget->item(row, mainCol)->text() == "OL." ) {
                curColor = QBrush(mFilterColorMain);
                catched = true;
            }
            else if ( mTableWidget->item(row, auxCol)->text() == "OL." ) {
                curColor = QBrush(mFilterColorAux);
                catched = true;
            }
            else if ( (main_value < ui->dSB_main_min->value() || main_value > ui->dSB_main_max->value())
                       && aux_isNotEmpty && (aux_value < ui->dSB_aux_min->value() || aux_value > ui->dSB_aux_max->value()) ) {
                curColor = QBrush(mFilterColorBoth);
                catched = true;
            }
            else if ( main_value < ui->dSB_main_min->value() || main_value > ui->dSB_main_max->value() ) {
                curColor = QBrush(mFilterColorMain);
                catched = true;
            }
            else if ( aux_isNotEmpty && (aux_value < ui->dSB_aux_min->value() || aux_value > ui->dSB_aux_max->value()) ) {
                curColor = QBrush(mFilterColorAux);
                catched = true;
            }
        }
        else if (ui->cB_Filter_main->isChecked()) {
            if ( mTableWidget->item(row, mainCol)->text() == "OL." ) {
                curColor = QBrush(mFilterColorMain);
                catched = true;
            }
            else if ( main_value < ui->dSB_main_min->value() || main_value > ui->dSB_main_max->value() ) {
                curColor = QBrush(mFilterColorMain);
                catched = true;
            }
            else {
                curColor = QBrush(mResetColor);
            }
        }
        else if (ui->cB_Filter_aux->isChecked()) {
            if ( mTableWidget->item(row, auxCol)->text() == "OL." ) {
                curColor = QBrush(mFilterColorAux);
                catched = true;
            }
            else if ( aux_isNotEmpty && (aux_value < ui->dSB_aux_min->value() || aux_value > ui->dSB_aux_max->value()) ) {
                curColor = QBrush(mFilterColorAux);
                catched = true;
            }
            else {
                curColor = QBrush(mResetColor);
            }
        }
        else {
            curColor = QBrush(mResetColor);
        }
    }


    for (int col = 0; col < mTableWidget->columnCount(); ++col)
    {
        if (mTableWidget->item(row, col)) {
            mTableWidget->item(row, col)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            mTableWidget->item(row, col)->setBackground(curColor);
        }
    }

    if (!mFreeScroll) {
        mTableWidget->scrollToBottom();
    }

    if ( catched == true ) {
        playSound();
    }

    mLastRow = row;
}

void DataStorage::onShowWindow(const bool &value)
{
    this->setHidden(!value);
}

void DataStorage::onRecordData(const bool &value)
{
    mRecordData = value;
    if (mRecordData) {
        ui->pB_Record->setIcon(setColoredSvg(":/image/Record_Data_Stop", QColor(229, 128, 255), ui->pB_Record->iconSize()));
    }
    else {
        ui->pB_Record->setIcon(setColoredSvg(":/image/Record_Data", QColor(229, 128, 255), ui->pB_Record->iconSize()));
    }
}

void DataStorage::onApplyFilter(void)
{
    mFilterData.mainActive = ui->cB_Filter_main->isChecked();
    mFilterData.auxActive  = ui->cB_Filter_aux->isChecked();
    mFilterData.inverted   = ui->cBInvertFilter->isChecked();
    mFilterData.main       = {ui->dSB_main_min->value(),ui->dSB_main_max->value()};
    mFilterData.aux        = {ui->dSB_aux_min->value(),ui->dSB_aux_max->value()};

    Q_EMIT(filterDataChanged(mFilterData));

    const int mainCol = mTableHeader.indexOf("Main Value");
    const int auxCol  = mTableHeader.indexOf("Aux Value");

    for (int row = 0; row < mTableWidget->rowCount(); ++row)
    {
        double main_value = mTableWidget->item(row, mainCol)->text().toDouble();
        double aux_value = mTableWidget->item(row, auxCol)->text().toDouble();
        bool aux_isNotEmpty = mTableWidget->item(row, auxCol)->text() == "" ? false : true;

        QBrush curColor;

        //Check Filter
        if (ui->cBInvertFilter->isChecked()) {
            if (ui->cB_Filter_main->isChecked() && ui->cB_Filter_aux->isChecked()) {
                if ( (main_value >= ui->dSB_main_min->value() && main_value <= ui->dSB_main_max->value())
                    && aux_isNotEmpty && (aux_value >= ui->dSB_aux_min->value() && aux_value <= ui->dSB_aux_max->value()) ) {
                    curColor = QBrush(mFilterColorBoth);
                }
                else if ( main_value >= ui->dSB_main_min->value() && main_value <= ui->dSB_main_max->value() ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else if ( aux_isNotEmpty && aux_value >= ui->dSB_aux_min->value() && aux_value <= ui->dSB_aux_max->value() ) {
                    curColor = QBrush(mFilterColorAux);
                }
            }
            else if (ui->cB_Filter_main->isChecked()) {
                if ( main_value >= ui->dSB_main_min->value() && main_value <= ui->dSB_main_max->value() ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else {
                    curColor = QBrush(mResetColor);
                }
            }
            else if (ui->cB_Filter_aux->isChecked()) {
                if ( aux_isNotEmpty && aux_value >= ui->dSB_aux_min->value() && aux_value <= ui->dSB_aux_max->value() ) {
                    curColor = QBrush(mFilterColorAux);
                }
                else {
                    curColor = QBrush(mResetColor);
                }
            }
            else {
                curColor = QBrush(mResetColor);
            }
        }
        else {
            if (ui->cB_Filter_main->isChecked() && ui->cB_Filter_aux->isChecked()) {
                if ( mTableWidget->item(row, mainCol)->text() == "OL."
                    && mTableWidget->item(row, auxCol)->text() == "OL." ) {
                    curColor = QBrush(mFilterColorBoth);
                }
                else if ( mTableWidget->item(row, mainCol)->text() == "OL." ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else if ( mTableWidget->item(row, auxCol)->text() == "OL." ) {
                    curColor = QBrush(mFilterColorAux);
                }
                else if ( (main_value < ui->dSB_main_min->value() || main_value > ui->dSB_main_max->value())
                           && aux_isNotEmpty && (aux_value < ui->dSB_aux_min->value() || aux_value > ui->dSB_aux_max->value()) ) {
                    curColor = QBrush(mFilterColorBoth);
                }
                else if ( main_value < ui->dSB_main_min->value() || main_value > ui->dSB_main_max->value() ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else if ( aux_isNotEmpty && (aux_value < ui->dSB_aux_min->value() || aux_value > ui->dSB_aux_max->value()) ) {
                    curColor = QBrush(mFilterColorAux);
                }
            }
            else if (ui->cB_Filter_main->isChecked()) {
                if ( mTableWidget->item(row, mainCol)->text() == "OL." ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else if ( main_value < ui->dSB_main_min->value() || main_value > ui->dSB_main_max->value() ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else {
                    curColor = QBrush(mResetColor);
                }
            }
            else if (ui->cB_Filter_aux->isChecked()) {
                if ( mTableWidget->item(row, auxCol)->text() == "OL." ) {
                    curColor = QBrush(mFilterColorAux);
                }
                else if ( aux_isNotEmpty && (aux_value < ui->dSB_aux_min->value() || aux_value > ui->dSB_aux_max->value()) ) {
                    curColor = QBrush(mFilterColorAux);
                }
                else {
                    curColor = QBrush(mResetColor);
                }
            }
            else {
                curColor = QBrush(mResetColor);
            }
        }

        for (int col = 0; col < mTableWidget->columnCount(); ++col)
        {
            mTableWidget->item(row, col)->setBackground(curColor);
        }
    }
}

void DataStorage::onSetColorFilter(const filterColor_s &color)
{
    // Helper lambda to apply an alpha channel of 0.1 to a color
    auto getAlphaColor = [](const QColor &color) {
        QColor c = color;
        c.setAlpha(0.2 * 255);
        return c;
    };
    mFilterColorMain = getAlphaColor(color.main);
    mFilterColorAux  = getAlphaColor(color.aux);
    mFilterColorBoth = mixColors(mFilterColorMain, mFilterColorAux);
    onApplyFilter();
}

QPixmap DataStorage::setColoredSvg(const QString &svgPath, const QColor &color, const QSize &size)
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

void DataStorage::playSound()
{
    if (mSoundAvailable == true && ui->cB_sound->isChecked() == true) {
        onStopSound();
        ma_device_start(&mSoundDevice);
        mSoundTimer.start();
    }
}

void DataStorage::copySelectionToClipboard()
{
    // Retrieve selected items from the table widget
    QList<QTableWidgetItem *> items = mTableWidget->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    // Map to store items by row and column indices
    QMap<int, QMap<int, QTableWidgetItem *>> cellMap;

    // Initialize bounds with the first item's coordinates
    int minRow = items.first()->row();
    int maxRow = items.first()->row();
    int minCol = items.first()->column();
    int maxCol = items.first()->column();

    // Populate the map and determine the bounding box of the selection
    for (QList<QTableWidgetItem *>::iterator it = items.begin(); it != items.end(); ++it)
    {
        QTableWidgetItem *item = *it;
        int r = item->row();
        int c = item->column();
        cellMap[r][c] = item;

        if (r < minRow) minRow = r;
        if (r > maxRow) maxRow = r;
        if (c < minCol) minCol = c;
        if (c > maxCol) maxCol = c;
    }

    // Prepare the string for clipboard content
    QString clipboardText;
    // Reserve memory to avoid multiple reallocations during string concatenation
    // Estimate size: (rows * cols) * (avg char length + separator)
    clipboardText.reserve((maxRow - minRow + 1) * (maxCol - minCol + 1) * 10);

    // Iterate through the bounding box to construct the tab-separated text
    for (int r = minRow; r <= maxRow; ++r) {
        // Use value() to avoid creating empty sub-maps for missing rows
        const auto& rowMap = cellMap.value(r);

        for (int c = minCol; c <= maxCol; ++c) {
            QTableWidgetItem *item = rowMap.value(c);
            if (item) {
                clipboardText += item->text();
            }
            // If item is null, append an empty string (handled by separator logic below)

            // Add tab separator between columns, but not after the last column
            if (c < maxCol) {
                clipboardText += '\t';
            }
        }

        // Add newline separator between rows, but not after the last row
        if (r < maxRow) {
            clipboardText += '\n';
        }
    }

    // Set the clipboard content
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(clipboardText);
}

QColor DataStorage::mixColors(const QColor &c1, const QColor &c2)
{
    int r = (c1.red()   + c2.red())   / 2;
    int g = (c1.green() + c2.green()) / 2;
    int b = (c1.blue()  + c2.blue())  / 2;
    int a = (c1.alpha() + c2.alpha()) / 2;

    return QColor(r, g, b, a);
}

void DataStorage::onVolumeChanged(int value)
{
    mSineWaveConfig = ma_waveform_config_init(
        mSoundDevice.playback.format, mSoundDevice.playback.channels,
        mSoundDevice.sampleRate, ma_waveform_type_sine,
        (double)value / 20.0, SOUND_FREQUENCY);
    ma_waveform_init(&mSineWaveConfig, &mSineWave);
}

void DataStorage::onStopSound()
{
    if (ma_device_is_started(&mSoundDevice)) {
        ma_device_stop(&mSoundDevice);
    }
}

void DataStorage::onScrollLock()
{
    mFreeScroll = !mFreeScroll;
    if (mFreeScroll) {
        ui->pB_lockScroll->setIcon(setColoredSvg(":/image/Scroll_ON", QColor(128, 255, 179), ui->pB_Record->iconSize()));
    }
    else {
        ui->pB_lockScroll->setIcon(setColoredSvg(":/image/Scroll_OFF", QColor(255, 127, 42), ui->pB_Record->iconSize()));
        mTableWidget->scrollToBottom();
    }
}

void DataStorage::closeEvent(QCloseEvent *event)
{
    Q_EMIT showStatusChanged(false);
    mWindowGeometry = this->saveGeometry();

    QMainWindow::closeEvent(event);
}

void DataStorage::hideEvent(QHideEvent *event)
{
    Q_EMIT(showStatusChanged(false));
    mWindowGeometry = this->saveGeometry();

    QMainWindow::hideEvent(event);
}

void DataStorage::showEvent(QShowEvent *event)
{
    /* Set Window size */
    if (mCurrentGeometry.isEmpty()) {
        QRect screen_geometry = mParent->screen()->availableGeometry();
        QPoint main_pos = mParent->pos();
        QSize main_size = mParent->size();

        mCurrentGeometry = QRect(
            mParent->x() + mParent->geometry().width(),
            mParent->geometry().top(),
            mColumnSize * (mTableHeader.size() + 1) + 40, mParent->geometry().height()
        );
        // Check enough room right of main window
        if (mCurrentGeometry.width() + mCurrentGeometry.x() < screen_geometry.width()) {
            // Check if enough room below
            if (mCurrentGeometry.height() + mCurrentGeometry.y() < screen_geometry.height()) {
                goto END;
            }
            // Check if enough room above
            else if (mCurrentGeometry.y() - mCurrentGeometry.height() > 0) {
                mCurrentGeometry.moveTop( mCurrentGeometry.height() - main_size.height() );
                goto END;
            }
        }
        // Check enough room left of main window
        else if (main_pos.x() - mCurrentGeometry.width() > 0) {
            // Check if enough room below
            if (mCurrentGeometry.height() + mCurrentGeometry.y() < screen_geometry.height()) {
                mCurrentGeometry.moveLeft(mParent->x() - mCurrentGeometry.width());
                goto END;
            }
            // Check if enough room above
            else if (mCurrentGeometry.y() - mCurrentGeometry.height() > 0) {
                mCurrentGeometry.setRect(
                    mParent->x() - mCurrentGeometry.width(),
                    mParent->geometry().top() - (mCurrentGeometry.height() - main_size.height()),
                    mCurrentGeometry.width(),
                    mCurrentGeometry.height()
                );
                goto END;
            }
        }

        // Not enough room to fit, we put it top left corner of the screen
        mCurrentGeometry.moveTopLeft(screen_geometry.topLeft() + QPoint(10, 10));

        if (mCurrentGeometry.height() > screen_geometry.height() - mCurrentGeometry.y()) {
            mCurrentGeometry.setHeight(screen_geometry.height() - 150 - mCurrentGeometry.y()); // 150 to add some room
        }
        if (mCurrentGeometry.width() > screen_geometry.width() - mCurrentGeometry.x()) {
            mCurrentGeometry.setWidth(screen_geometry.width() - 150 - mCurrentGeometry.x()); // 150 to add some room
        }

    END:
        this->setGeometry(mCurrentGeometry);
        mWindowGeometry = this->saveGeometry();
    }

    Q_EMIT(showStatusChanged(true));
    this->restoreGeometry(mWindowGeometry);
    QMainWindow::showEvent(event);
}

void DataStorage::keyPressEvent(QKeyEvent *event)
{
    // Intercept Ctrl+C
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        copySelectionToClipboard();
        return; // Avoid default behavior
    }

    QMainWindow::keyPressEvent(event);
}
