/**
 * @file barscale.h
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */
#ifndef BARSCALE_H
#define BARSCALE_H

#include <QProgressBar>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

class BarScale : public QProgressBar {

    Q_OBJECT

public:
    explicit BarScale(QWidget *parent = nullptr) {
        (void)parent;
    }
    // ~BarScale();
    void setColor(QColor color) {
        barColor = color;
        int r, g, b;
        barColor.getRgb(&r, &g, &b);

        QString styleSheet = QString(
                                 "QProgressBar::label {"
                                 "    color: rgb(%1, %2, %3);"
                                 "}"
                                 "QProgressBar::chunk {"
                                 "    background-color: #005090;"
                                 "}"
                                 ).arg(r).arg(g).arg(b);
        this->setStyleSheet(styleSheet);
    }

private:
    QColor barColor = QColor(255, 204, 0);

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        int max = maximum();
        int val = value();
        if (max <= 0) return;

        double totalWidth = rect().width();
        double height = rect().height() / 2.0;

        double stepWidth = totalWidth / max;
        double rectWidth = stepWidth;

        if (rectWidth < 1.0) rectWidth = 1.0;

        // Calculate font for labels
        QFont font = painter.font();
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(barColor);
        QFontMetrics fm(painter.font());

        double rectHeight = height / 2.5;
        double rectScaleFactor = 3.0f;
        double diamondHeight = height / 1.5;
        double diamondScaleFactor = 2.0f;

        // We draw the scale bar axis first
        for (int i = 0; i < max; ++i) {
            double startX = i * stepWidth;
            QRectF rectRect(startX + stepWidth / 2.0 - rectWidth / (rectScaleFactor * 2.0), fm.height() / 2.0, rectWidth / rectScaleFactor, rectHeight);
            QRectF textRect(startX, 0, rectWidth, height);

            if (i == max - 1) { // Nothing on last segment
                painter.fillRect(textRect, Qt::transparent);
            }
            else if (i % 8 == 0) { // Digit
                QString labelText = QString::number(i / 8);
                QFontMetrics fm(painter.font());

                double x = textRect.center().x() - fm.horizontalAdvance(labelText) / 2.0;
                double y = textRect.top() + fm.ascent();

                painter.drawText(QPointF(x, y), labelText);
            }
            else if (i % 4 == 0) { // Diamond
                // Center of the segment
                double centerX = startX + stepWidth / 2.0;
                double centerY = rectHeight / 2.0 + (fm.height() / 2.0);

                // Diamond points
                QPointF top(centerX, centerY - diamondHeight / 2.0);
                QPointF right(centerX + rectWidth / (diamondScaleFactor * 2.0), centerY);
                QPointF bottom(centerX, centerY + diamondHeight / 2.0);
                QPointF left(centerX - rectWidth / (diamondScaleFactor * 2.0), centerY);

                QPainterPath diamondPath;
                diamondPath.moveTo(top);
                diamondPath.lineTo(right);
                diamondPath.lineTo(bottom);
                diamondPath.lineTo(left);
                diamondPath.closeSubpath();

                painter.fillPath(diamondPath, barColor);
            }
            else { // Rectangle
                painter.fillRect(rectRect, barColor);
            }
        }

        // We draw the scale bar next
        double gap = 6.0;
        double traitWidth = stepWidth - gap;

        for (int i = 0; i < max; ++i) {
            QRectF traitRect(i * stepWidth + gap / 2.0, height, traitWidth, height);

            if (i == max - 1 && i < val) { // Triangle on last segment (overflow)
                double startX = i * stepWidth + gap / 2.0;
                double endX = startX + stepWidth - gap / 2.0;
                QPainterPath trianglePath;
                trianglePath.moveTo(startX, height);
                trianglePath.lineTo(startX, rect().height());
                trianglePath.lineTo(endX, rect().height() - height / 2.0);
                trianglePath.closeSubpath();

                painter.fillPath(trianglePath, barColor);
            }
            else if (i < val) {
                double startX = i * stepWidth;
                // Center of the segment
                double centerX = startX + stepWidth / 2.0;

                double triangleHeight = rectHeight / 2.5;
                double baseHeight = height - triangleHeight;

                double arrowFactor = 1.25f;
                if (i % 8 == 0) {
                    arrowFactor = 1.0f;
                }

                /*
                 *   4
                 *  / \
                 * 5   3
                 * |   |
                 * 6-1-2
                */
                QPointF pt_1(centerX, rect().height());
                QPointF pt_2(centerX + (traitWidth / 2.0 / arrowFactor), rect().height());
                QPointF pt_3(centerX + (traitWidth / 2.0 / arrowFactor), rect().height() - (baseHeight / arrowFactor));
                QPointF pt_4(centerX, rect().height() - (height / arrowFactor));
                QPointF pt_5(centerX - (traitWidth / 2.0 / arrowFactor), rect().height() - (baseHeight / arrowFactor));
                QPointF pt_6(centerX - (traitWidth / 2.0 / arrowFactor), rect().height());

                QPainterPath arrowPath;
                arrowPath.moveTo(pt_1);
                arrowPath.lineTo(pt_2);
                arrowPath.lineTo(pt_3);
                arrowPath.lineTo(pt_4);
                arrowPath.lineTo(pt_5);
                arrowPath.lineTo(pt_6);
                arrowPath.lineTo(pt_1);
                arrowPath.closeSubpath();

                painter.fillPath(arrowPath, barColor);
            }
            else {
                painter.fillRect(traitRect, Qt::transparent);
            }
        }
    }
};


#endif // BARSCALE_H
