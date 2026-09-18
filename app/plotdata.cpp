/**
 * @file plotdata.cpp
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */
#include "plotdata.h"
#include <QtGui/qevent.h>
#include <QFile>
#include <QPainter>
#include <QSvgRenderer>
#include <QClipboard>
#include "ui_plotdata.h"

PlotData::PlotData(QWidget *parent)
    : QWidget(parent), ui(new Ui::PlotData)
    , mParent(parent)
{
    ui->setupUi(this);

    setWindowTitle(QString("%1 - Plot Data").arg(_TARGET));

    // Set table property
    mTable = ui->tWData;
    mTable->setColumnCount(mTableHeader.size());
    mTable->setHorizontalHeaderLabels(mTableHeader);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int i = 0; i < mTableHeader.size(); ++i) {
        if (i == mTableHeader.indexOf("X"))
            mTable->setColumnWidth(i,mColumnSize / 1);
        else
            mTable->setColumnWidth(i,mColumnSize);
    }

    this->setFixedWidth(mTableHeader.size() * mColumnSize * 1.3);

    int r, g, b;
    bgColor = getParentBackgroundColor(this);
    mResetColor = bgColor;
    bgColor.getRgb(&r, &g, &b);
    QColor buttonHoverColor = adjustColorProportional(bgColor, 2.0);
    int cr, cg, cb, ca;
    buttonHoverColor.getRgb(&cr, &cg, &cb, &ca);

    QString widgetStyleSheet =
        QString(
            "QPushButton {"
            "    border: none;"
            "    background-color: rgba(%1, %2, %3, 200);"
            "}"
            "QPushButton:hover {"
            "    background-color: rgba(%4, %5, %6, %7);"
            "}"
            "QPushButton:pressed {"
            "    background-color: rgba(150, 150, 150, 80);"
            "}"
            "background-color: rgba(%1, %2, %3, 200);"
        ).arg(r).arg(g).arg(b).arg(cr).arg(cg).arg(cb).arg(ca);
    this->setStyleSheet(widgetStyleSheet);

    mTable->setStyleSheet("border: none;");
    mTable->viewport()->setStyleSheet(QString("background-color: rgba(%1, %2, %3, 0);").arg(r).arg(g).arg(b));

    ui->pBClose->setText ("");
    ui->pBClose->setIcon(setColoredSvg(":/image/Close", QColor(255, 0, 0), ui->pBClose->iconSize()));
    QObject::connect(ui->pBClose, &QPushButton::clicked, this, &QWidget::close);

    ui->pBScroll->setText ("");
    ui->pBScroll->setIcon(setColoredSvg(":/image/Scroll_OFF", QColor(255, 127, 42), ui->pBScroll->iconSize()));
    QObject::connect(ui->pBScroll, &QPushButton::clicked, this, [=, this] () {
        mFreeScroll = !mFreeScroll;
        if (mFreeScroll)
            ui->pBScroll->setIcon(setColoredSvg(":/image/Scroll_ON", QColor(128, 255, 179), ui->pBScroll->iconSize()));
        else
            ui->pBScroll->setIcon(setColoredSvg(":/image/Scroll_OFF", QColor(255, 127, 42), ui->pBScroll->iconSize()));
    });
}

PlotData::~PlotData()
{
    delete ui;
}

void PlotData::append(const qint64 &time, const BM86xDataType_s &data)
{
    if (data.value.m_overflow == true && data.value.a_overflow == true) {
        return;
    }

    int row = mTable->rowCount();

    QString xValue, y1Value, y2Value;
    QLocale locale;
    xValue = locale.toString(time);

    if (data.value.m_overflow == false) {
        y1Value = locale.toString(data.value.m_value, 'g', 6);
    }

    if (data.value.a_overflow == false && data.value.a_unit != last_unit) {
        y2Value = locale.toString(data.value.a_value, 'g', 4);
    }

    if (y1Value.isEmpty() && y2Value.isEmpty()) {
        return;
    }

    mTable->insertRow(row);

    QMap<QString, QString> values = { {"X", xValue},
                                     {"Y1", y1Value},
                                     {"Y2", y2Value},
                                     };

    for (int i = 0; i < mTableHeader.size(); ++i) {
        const QString &header = mTableHeader.at(i);

        if (values.contains(header))
            mTable->setItem(row, i, new QTableWidgetItem(values.value(header)));
    }

    const int mainCol = mTableHeader.indexOf("Y1");
    const double main_value = data.value.m_value;
    const int auxCol = mTableHeader.indexOf("Y2");
    const double aux_value = data.value.a_value;
    bool aux_isNotEmpty = mTable->item(row, auxCol)->text() == "" ? false : true;

    QBrush curColor;

    //Check Filter
    if (mFilterValue.inverted) {
        if (mFilterValue.mainActive && mFilterValue.auxActive) {
            if ( (main_value >= mFilterValue.main.min && main_value <= mFilterValue.main.max)
                && aux_isNotEmpty && (aux_value >= mFilterValue.aux.min && aux_value <= mFilterValue.aux.max) ) {
                curColor = QBrush(mFilterColorBoth);
            }
            else if ( main_value >= mFilterValue.main.min && main_value <= mFilterValue.main.max ) {
                curColor = QBrush(mFilterColorMain);
            }
            else if ( aux_isNotEmpty && aux_value >= mFilterValue.aux.min && aux_value <= mFilterValue.aux.max ) {
                curColor = QBrush(mFilterColorAux);
            }
        }
        else if (mFilterValue.mainActive) {
            if ( main_value >= mFilterValue.main.min && main_value <= mFilterValue.main.max ) {
                curColor = QBrush(mFilterColorMain);
            }
            else {
                curColor = QBrush(mResetColor);
            }
        }
        else if (mFilterValue.auxActive) {
            if ( aux_isNotEmpty && aux_value >= mFilterValue.aux.min && aux_value <= mFilterValue.aux.max ) {
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
        if (mFilterValue.mainActive && mFilterValue.auxActive) {
            if ( mTable->item(row, mainCol)->text() == "OL."
                && mTable->item(row, auxCol)->text() == "OL." ) {
                curColor = QBrush(mFilterColorBoth);
            }
            else if ( mTable->item(row, mainCol)->text() == "OL." ) {
                curColor = QBrush(mFilterColorMain);
            }
            else if ( mTable->item(row, auxCol)->text() == "OL." ) {
                curColor = QBrush(mFilterColorAux);
            }
            else if ( (main_value < mFilterValue.main.min || main_value > mFilterValue.main.max)
                     && aux_isNotEmpty && (aux_value < mFilterValue.aux.min || aux_value > mFilterValue.aux.max) ) {
                curColor = QBrush(mFilterColorBoth);
            }
            else if ( main_value < mFilterValue.main.min || main_value > mFilterValue.main.max ) {
                curColor = QBrush(mFilterColorMain);
            }
            else if ( aux_isNotEmpty && (aux_value < mFilterValue.aux.min || aux_value > mFilterValue.aux.max) ) {
                curColor = QBrush(mFilterColorAux);
            }
        }
        else if (mFilterValue.mainActive) {
            if ( mTable->item(row, mainCol)->text() == "OL." ) {
                curColor = QBrush(mFilterColorMain);
            }
            else if ( main_value < mFilterValue.main.min || main_value > mFilterValue.main.max ) {
                curColor = QBrush(mFilterColorMain);
            }
            else {
                curColor = QBrush(mResetColor);
            }
        }
        else if (mFilterValue.auxActive) {
            if ( mTable->item(row, auxCol)->text() == "OL." ) {
                curColor = QBrush(mFilterColorAux);
            }
            else if ( aux_isNotEmpty && (aux_value < mFilterValue.aux.min || aux_value > mFilterValue.aux.max) ) {
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

    for (int col = 0; col < mTable->columnCount(); ++col)
    {
        if (mTable->item(row, col)) {
            mTable->item(row, col)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            mTable->item(row, col)->setBackground(curColor);
        }
    }

    if (!mFreeScroll) {
        mTable->scrollToBottom();
    }
}

void PlotData::clear()
{
    mTable->clearContents();
    mTable->setRowCount(0);
}

void PlotData::applyFilter()
{
    const int mainCol = mTableHeader.indexOf("Y1");
    const int auxCol = mTableHeader.indexOf("Y2");


    for (int row = 0; row < mTable->rowCount(); ++row)
    {
        double main_value = mTable->item(row, mainCol)->text().toDouble();
        double aux_value = mTable->item(row, auxCol)->text().toDouble();
        bool aux_isNotEmpty = mTable->item(row, auxCol)->text() == "" ? false : true;

        QBrush curColor;

        //Check Filter

        if (mFilterValue.inverted) {
            if (mFilterValue.mainActive && mFilterValue.auxActive) {
                if ( (main_value >= mFilterValue.main.min && main_value <= mFilterValue.main.max)
                    && aux_isNotEmpty && (aux_value >= mFilterValue.aux.min && aux_value <= mFilterValue.aux.max) ) {
                    curColor = QBrush(mFilterColorBoth);
                }
                else if ( main_value >= mFilterValue.main.min && main_value <= mFilterValue.main.max ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else if ( aux_isNotEmpty && aux_value >= mFilterValue.aux.min && aux_value <= mFilterValue.aux.max ) {
                    curColor = QBrush(mFilterColorAux);
                }
            }
            else if (mFilterValue.mainActive) {
                if ( main_value >= mFilterValue.main.min && main_value <= mFilterValue.main.max ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else {
                    curColor = QBrush(mResetColor);
                }
            }
            else if (mFilterValue.auxActive) {
                if ( aux_isNotEmpty && aux_value >= mFilterValue.aux.min && aux_value <= mFilterValue.aux.max ) {
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
            if (mFilterValue.mainActive && mFilterValue.auxActive) {
                if ( mTable->item(row, mainCol)->text() == "OL."
                    && mTable->item(row, auxCol)->text() == "OL." ) {
                    curColor = QBrush(mFilterColorBoth);
                }
                else if ( mTable->item(row, mainCol)->text() == "OL." ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else if ( mTable->item(row, auxCol)->text() == "OL." ) {
                    curColor = QBrush(mFilterColorAux);
                }
                else if ( (main_value < mFilterValue.main.min || main_value > mFilterValue.main.max)
                         && aux_isNotEmpty && (aux_value < mFilterValue.aux.min || aux_value > mFilterValue.aux.max) ) {
                    curColor = QBrush(mFilterColorBoth);
                }
                else if ( main_value < mFilterValue.main.min || main_value > mFilterValue.main.max ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else if ( aux_isNotEmpty && (aux_value < mFilterValue.aux.min || aux_value > mFilterValue.aux.max) ) {
                    curColor = QBrush(mFilterColorAux);
                }
            }
            else if (mFilterValue.mainActive) {
                if ( mTable->item(row, mainCol)->text() == "OL." ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else if ( main_value < mFilterValue.main.min || main_value > mFilterValue.main.max ) {
                    curColor = QBrush(mFilterColorMain);
                }
                else {
                    curColor = QBrush(mResetColor);
                }
            }
            else if (mFilterValue.auxActive) {
                if ( mTable->item(row, auxCol)->text() == "OL." ) {
                    curColor = QBrush(mFilterColorAux);
                }
                else if ( aux_isNotEmpty && (aux_value < mFilterValue.aux.min || aux_value > mFilterValue.aux.max) ) {
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

        for (int col = 0; col < mTable->columnCount(); ++col)
        {
            mTable->item(row, col)->setBackground(curColor);
        }
    }
}

void PlotData::onSetColorFilter(const DataStorage::filterColor_s &color)
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
    applyFilter();
}

QPixmap PlotData::setColoredSvg(const QString &svgPath, const QColor &color, const QSize &size)
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

void PlotData::copySelectionToClipboard()
{
    // Retrieve selected items from the table widget
    QList<QTableWidgetItem *> items = mTable->selectedItems();
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

void PlotData::keyPressEvent(QKeyEvent *event)
{
    // Intercept Ctrl+C
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        copySelectionToClipboard();
        return; // Avoid default behavior
    }

    QWidget::keyPressEvent(event);
}
