/**
 * @file plotdata.h
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */
#ifndef PLOTDATA_H
#define PLOTDATA_H

#include <QWidget>
#include <QTableWidget>
#include "shortcutaction.h"
#include "datastorage.h"

namespace Ui {
class PlotData;
}

class PlotData : public QWidget
{
    Q_OBJECT

public:
    explicit PlotData(QWidget *parent = nullptr);
    ~PlotData();

    void applyFilter ();

public Q_SLOTS:
    void append(const qint64 &time, const BM86xDataType_s &data);
    void clear();
    void onSetColorFilter (const DataStorage::filterColor_s &color);
    void setFilter(const DataStorage::filterData_s &filter) { mFilterValue = filter; applyFilter(); }

private:
    Ui::PlotData *ui;

    DataStorage::filterData_s mFilterValue;

    int  mColumnSize = 90;
    bool mFreeScroll = false;
    QColor bgColor = Qt::transparent;
    QPointer<QTableWidget>  mTable;
    QPointer<QWidget>       mParent;
    QList<Shortcut::Action> mActionShortcutList;
    const QStringList mTableHeader = {"X","Y1","Y2"};
    QColor mFilterColorBoth = QColor(255, 0, 0, 50);
    QColor mFilterColorMain = QColor(255, 0, 255, 50);
    QColor mFilterColorAux  = QColor(255, 255, 0, 50);
    QColor mResetColor      = Qt::transparent;

    QPixmap setColoredSvg (const QString& svgPath, const QColor& color, const QSize& size);
    void copySelectionToClipboard();

    QColor mixColors(const QColor &c1, const QColor &c2) {
        int r = (c1.red()   + c2.red())   / 2;
        int g = (c1.green() + c2.green()) / 2;
        int b = (c1.blue()  + c2.blue())  / 2;
        int a = (c1.alpha() + c2.alpha()) / 2;

        return QColor(r, g, b, a);
    }

    /**
     * @brief Retrieves the background color of the parent widget.
     *
     * This method accesses the QPalette of the parent to find the 'Base' or 'Window' role,
     * which represents the standard background color used by the parent container.
     *
     * @return QColor The background color of the parent, or Qt::transparent if no parent exists.
     */
    QColor getParentBackgroundColor(QWidget* widget) {
        if (!widget || !widget->parentWidget()) {
            return Qt::transparent;
        }

        // Access the palette of the parent widget
        const QPalette& parentPalette = widget->parentWidget()->palette();

        // 'Window' is the standard color for the background of a top-level widget/container.
        // 'Base' is often used for text areas (like QLineEdit), but 'Window' is more appropriate for general backgrounds.
        QColor bgColor = parentPalette.color(QPalette::Window);

        // Fallback: if the color is invalid or not set, return transparent
        return bgColor.isValid() ? bgColor : Qt::transparent;
    }

    /**
     * @brief Adjusts color brightness using a percentage-based approach.
     *
     * Instead of adding/subtracting fixed integers, this method uses a multiplier.
     * This is the industry standard for UI design (e.g., CSS filters or Material Design)
     * because it scales proportionally with the existing luminance.
     *
     * @param color The source QColor.
     * @param factor A multiplier: > 1.0 to lighten, < 1.0 to darken.
     *               Example: 1.2 increases brightness by 20%, 0.8 decreases it by 20%.
     * @return QColor The adjusted color.
     */
    QColor adjustColorProportional(const QColor& color, qreal factor) {
        // We use qreal (double) for precision during multiplication to avoid rounding errors
        int r = qRound(color.red()   * factor);
        int g = qRound(color.green() * factor);
        int b = qRound(color.blue()  * factor);

        // Clamp values between 0 and 255 to prevent overflow/underflow
        r = std::clamp(r, 0, 255);
        g = std::clamp(g, 0, 255);
        b = std::clamp(b, 0, 255);

        return QColor(r, g, b, color.alpha());
    }

protected:
    void keyPressEvent(QKeyEvent *event) override;
};

#endif // PLOTDATA_H
