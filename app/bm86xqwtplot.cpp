/**
 * @file bm86xqwtplot.cpp
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#include "bm86xqwtplot.h"

BM86xQwtPlot::BM86xQwtPlot(QWidget *)
    : QwtPlot()
{
    init();

    // QwtPlotZoomer
    //
    // Left drag             : Zoom in
    // Right click           : Zoom out one level
    // Ctrl + Right click    : Reset zoom
    zoomer = new DualAxisZoomer( this->canvas(), true, false);
    zoomer->setMousePattern(QwtEventPattern::MouseSelect2,
                            Qt::RightButton, Qt::ControlModifier);
    zoomer->setMousePattern(QwtEventPattern::MouseSelect3,
                            Qt::RightButton);
    zoomer->setRubberBandPen(QColor(Qt::color0));
    zoomer->setTrackerPen(QColor(Qt::color0));

    panner = new QwtPlotPanner( this->canvas());
    panner->setMouseButton(Qt::MiddleButton);

    // Wheel       zoom in/out Y axis
    // Shift+Whell zoom in/out X axis
    // Ctrl+Wheel  zoom in/out
    magnifierY = new QwtPlotMagnifier(  this->canvas() );
    magnifierY->setAxisEnabled(QwtPlot::xBottom, false);
    magnifierY->setMouseButton( Qt::NoButton );

    magnifierX = new QwtPlotMagnifier(  this->canvas() );
    magnifierX->setAxisEnabled(QwtPlot::yLeft, false);
    magnifierX->setAxisEnabled(QwtPlot::yRight, false);
    magnifierX->setMouseButton( Qt::NoButton );
    magnifierX->setWheelModifiers( Qt::ShiftModifier );

    magnifierXY = new QwtPlotMagnifier(  this->canvas() );
    magnifierXY->setMouseButton( Qt::NoButton );
    magnifierXY->setWheelModifiers( Qt::ControlModifier );

    time  = QDateTime::currentDateTime();

    m_positionLabel.attach(this);

    // Enable mouse tracking on the canvas
    canvas()->setMouseTracking(true);

    // Install event filter to capture mouse move events
    canvas()->installEventFilter(this);

    dt.setTimeZone(tz);
    savePlotBoundary();
}

/* ==============================================================================
 * Public slots
 * ==============================================================================
 */
void BM86xQwtPlot::onAppendData(const BM86xDataType_s &data) {
    lastDmmData = data;
    if (main_previousMode != data.value.m_mode || main_previousUnit != data.value.m_unit)
    {
        onClearData();
        //Set Main Unit
        if (data.value.m_mode == Res || data.value.m_mode == Cont) {
            if (data.value.m_unit == ohm_unit) {
                this->setAxisTitle(QwtPlot::yLeft,"\u03A9");
            }
            else if (data.value.m_unit == Kohm_unit) {
                this->setAxisTitle(QwtPlot::yLeft,"k\u03A9");
            }
            else {
                this->setAxisTitle(QwtPlot::yLeft,"M\u03A9");
            }
        }
        else {
            this->setAxisTitle(QwtPlot::yLeft,data.value.m_unitString);
        }

        //Set Aux Unit
        if (data.value.a_mode == Res || data.value.a_mode == Cont) {
            if (data.value.a_unit == ohm_unit) {
                this->setAxisTitle(QwtPlot::yRight,"\u03A9");
            }
            else if (data.value.a_unit == Kohm_unit) {
                this->setAxisTitle(QwtPlot::yRight,"k\u03A9");
            }
            else {
                this->setAxisTitle(QwtPlot::yRight,"M\u03A9");
            }
        }
        else {
            this->setAxisTitle(QwtPlot::yRight,data.value.a_unitString);
        }
    }

    if ((data.value.peakMode != None && previousPeak == None)
        || (data.value.peakMode == None && previousPeak != None))
        onClearData();

    main_previousUnit = data.value.m_unit;
    main_previousMode = data.value.m_mode;
    aux_previousUnit = data.value.a_unit;
    aux_previousMode = data.value.a_mode;
    previousPeak = data.value.peakMode;

    qint64 timeStamp = data.time.toMSecsSinceEpoch() - time.toMSecsSinceEpoch();
    if (timeStamp < 0)
        timeStamp = 0;

    if (data.value.m_overflow == false) {
        main_plot_data.append(QPointF(timeStamp, data.value.m_value));
    }

    if (data.value.a_overflow == false) {
        aux_plot_data.append(QPointF(timeStamp, data.value.a_value));
    }

    // Check Value Aux
    if (data.value.a_mode == last_mode) {
        aux_curve->setVisible(false);
        this->enableAxis(QwtPlot::yRight, false);
        if (m_auxZoneCenter) m_auxZoneCenter->hide();
        if (m_auxZoneOutsideLow) m_auxZoneOutsideLow->hide();
        if (m_auxZoneOutsideHigh) m_auxZoneOutsideHigh->hide();
    } else {
        aux_curve->setVisible(aux_show);
        this->enableAxis(QwtPlot::yRight, aux_show);
        onFilterDataChanged(mLastFilterData);
    }

    if(m_pauseStatus == false) {
        qint64 index = findMinIndex(main_plot_data, timeStamp - m_plotScale);
        if (index < 0)
            index = 0;

        this->setPlotScale(m_plotScale, false);

        /* Add space on Y axis */
        if (data.value.m_overflow == false)
        {
            AxisBoundary boundary;

            if (m_plotScale > BM86xPlot::ScaleFull) {
                QVector<QPointF> temp_data = main_plot_data.mid(index);
                boundary = getBoundaryY(temp_data);
            }
            else {
                boundary = getBoundaryY(main_plot_data);
            }

            double val_diff = boundary.max - boundary.min;
            double adjust;

            if ((qAbs(val_diff)) == 0) {
                adjust = 0.6;
            }
            else {
                adjust = val_diff / 10.0;
            }

            this->setAxisScale(QwtPlot::yLeft, boundary.min - adjust, boundary.max + adjust);
        }

        if (data.value.a_overflow == false)
        {
            AxisBoundary boundary;

            if (m_plotScale > BM86xPlot::ScaleFull) {
                QVector<QPointF> temp_data = aux_plot_data.mid(index);
                boundary = getBoundaryY(temp_data);
            }
            else {
                boundary = getBoundaryY(aux_plot_data);
            }

            double val_diff = boundary.max - boundary.min;
            double adjust;

            if ((qAbs(val_diff)) == 0) {
                adjust = 0.6;
            }
            else {
                adjust = val_diff / 10.0;
            }

            this->setAxisScale(QwtPlot::yRight, boundary.min - adjust, boundary.max + adjust);
        }

        zoomer->setZoomBase();
        this->replot();
        savePlotBoundary();
    }
}

void BM86xQwtPlot::onClearData() {
    main_plot_data.clear();
    aux_plot_data.clear();

    init();

    time  = QDateTime::currentDateTime();
    zoomer->setZoomBase();
    this->replot();
}

void BM86xQwtPlot::onFilterDataChanged(const DataStorage::filterData_s &value) {
    // Use a default no-pen style to avoid ambiguity with Qt::NoPen
    QPen tPen = Qt::NoPen;

    // Helper lambda to apply an alpha channel of 0.1 to a color
    auto getAlphaColor = [](const QColor &color) {
        QColor c = color;
        c.setAlpha(0.1 * 255);
        return c;
    };

    mLastFilterData = value;

    // --- MAIN ZONE MANAGEMENT ---
    if (value.mainActive && main_show) {
        QColor mainCol = getAlphaColor(mMainColor);
        // Retrieve the visible bounds of the Y-axis
        const double yMinVisible = axisScaleDiv(QwtPlot::yLeft).lowerBound();
        const double yMaxVisible = axisScaleDiv(QwtPlot::yLeft).upperBound();

        if (value.inverted) {
            // CASE INVERTED: Highlight the zone BETWEEN min and max
            if (!m_mainZoneCenter) {
                m_mainZoneCenter = new QwtPlotZoneItem();
                m_mainZoneCenter->setOrientation(Qt::Horizontal);
                m_mainZoneCenter->attach(this);
            }

            m_mainZoneCenter->show();
            m_mainZoneCenter->setBrush(mainCol);
            m_mainZoneCenter->setPen(tPen);
            m_mainZoneCenter->setInterval(value.main.min, value.main.max);

            // Hide outer zones as they are not active in this mode
            if (m_mainZoneOutsideLow) m_mainZoneOutsideLow->hide();
            if (m_mainZoneOutsideHigh) m_mainZoneOutsideHigh->hide();

        } else {
            // CASE NON-INVERTED: Highlight the OUTSIDE of the range (two separate zones)

            // LOW ZONE: From -infinity to min
            if (!m_mainZoneOutsideLow) {
                m_mainZoneOutsideLow = new QwtPlotZoneItem();
                m_mainZoneOutsideLow->setOrientation(Qt::Horizontal);
                m_mainZoneOutsideLow->attach(this);
            }

            // Clamp the upper bound to min, but ensure it doesn't exceed the visible axis max
            double lowMax = qMin(value.main.min, yMaxVisible);
            double lowMin = yMinVisible; // Start from the bottom of the visible graph

            if (lowMin < lowMax) {
                m_mainZoneOutsideLow->show();
                m_mainZoneOutsideLow->setBrush(mainCol);
                m_mainZoneOutsideLow->setPen(tPen);
                m_mainZoneOutsideLow->setInterval(lowMin, lowMax);
            } else {
                m_mainZoneOutsideLow->hide();
            }

            // HIGH ZONE: From max to +infinity
            if (!m_mainZoneOutsideHigh) {
                m_mainZoneOutsideHigh = new QwtPlotZoneItem();
                m_mainZoneOutsideHigh->setOrientation(Qt::Horizontal);
                m_mainZoneOutsideHigh->attach(this);
            }

            // Clamp the lower bound to max, but ensure it doesn't go below the visible axis min
            double highMin = qMax(value.main.max, yMinVisible);
            double highMax = yMaxVisible; // End at the top of the visible graph

            if (highMin < highMax) {
                m_mainZoneOutsideHigh->show();
                m_mainZoneOutsideHigh->setBrush(mainCol);
                m_mainZoneOutsideHigh->setPen(tPen);
                m_mainZoneOutsideHigh->setInterval(highMin, highMax);
            } else {
                m_mainZoneOutsideHigh->hide();
            }

            // Hide the center zone if it exists
            if (m_mainZoneCenter) m_mainZoneCenter->hide();
        }
    } else {
        // MAIN INACTIVE: Hide all associated zones
        if (m_mainZoneCenter) m_mainZoneCenter->hide();
        if (m_mainZoneOutsideLow) m_mainZoneOutsideLow->hide();
        if (m_mainZoneOutsideHigh) m_mainZoneOutsideHigh->hide();
    }

    // --- AUX ZONE MANAGEMENT ---
    if (value.auxActive && aux_show) {
        QColor auxCol = getAlphaColor(mAuxColor);
        const QwtScaleMap masterYMap = this->canvasMap(QwtPlot::yLeft);
        const QwtScaleMap slaveYMap = this->canvasMap(QwtPlot::yRight);

        // Retrieve the visible bounds of the Y-axis
        const double yMinVisible = axisScaleDiv(QwtPlot::yLeft).lowerBound();
        const double yMaxVisible = axisScaleDiv(QwtPlot::yLeft).upperBound();

        const double p1y = slaveYMap.transform(value.aux.max);
        const double p2y = slaveYMap.transform(value.aux.min);

        double y1 = masterYMap.invTransform(p1y);
        double y2 = masterYMap.invTransform(p2y);

#ifdef QT_DEBUG
        // qDebug() << "        slaveYMap :" << slaveYMap;
        // qDebug() << "       masterYMap :" << masterYMap;
        // qDebug() << "              p1y :" << p1y;
        // qDebug() << "              p2y :" << p2y;
        // qDebug() << "               y1 :" << y1;
        // qDebug() << "               y2 :" << y2;
        // qDebug() << "";
#endif

        if (value.inverted) {
            // CASE INVERTED: Highlight the zone BETWEEN aux.min and aux.max
            if (!m_auxZoneCenter) {
                m_auxZoneCenter = new QwtPlotZoneItem();
                m_auxZoneCenter->setOrientation(Qt::Horizontal);
                m_auxZoneCenter->attach(this);
            }

            m_auxZoneCenter->show();
            m_auxZoneCenter->setBrush(auxCol);
            m_auxZoneCenter->setPen(tPen);
            m_auxZoneCenter->setInterval(y2, y1);

            // Hide outer zones
            if (m_auxZoneOutsideLow) m_auxZoneOutsideLow->hide();
            if (m_auxZoneOutsideHigh) m_auxZoneOutsideHigh->hide();

        } else {
            // CASE NON-INVERTED: Two outer zones

            // LOW ZONE
            if (!m_auxZoneOutsideLow) {
                m_auxZoneOutsideLow = new QwtPlotZoneItem();
                m_auxZoneOutsideLow->setOrientation(Qt::Horizontal);
                m_auxZoneOutsideLow->attach(this);
            }

            double lowMax = qMin(y2, yMaxVisible);
            double lowMin = yMinVisible;

            if (lowMin < lowMax) {
                m_auxZoneOutsideLow->show();
                m_auxZoneOutsideLow->setBrush(auxCol);
                m_auxZoneOutsideLow->setPen(tPen);
                m_auxZoneOutsideLow->setInterval(lowMin, lowMax);
            } else {
                m_auxZoneOutsideLow->hide();
            }

            // HIGH ZONE
            if (!m_auxZoneOutsideHigh) {
                m_auxZoneOutsideHigh = new QwtPlotZoneItem();
                m_auxZoneOutsideHigh->setOrientation(Qt::Horizontal);
                m_auxZoneOutsideHigh->attach(this);
            }

            double highMin = qMax(y1, yMinVisible);
            double highMax = yMaxVisible;

            if (highMin < highMax) {
                m_auxZoneOutsideHigh->show();
                m_auxZoneOutsideHigh->setBrush(auxCol);
                m_auxZoneOutsideHigh->setPen(tPen);
                m_auxZoneOutsideHigh->setInterval(highMin, highMax);
            } else {
                m_auxZoneOutsideHigh->hide();
            }

            if (m_auxZoneCenter) m_auxZoneCenter->hide();
        }
    } else {
        if (m_auxZoneCenter) m_auxZoneCenter->hide();
        if (m_auxZoneOutsideLow) m_auxZoneOutsideLow->hide();
        if (m_auxZoneOutsideHigh) m_auxZoneOutsideHigh->hide();
    }

    this->replot();
}

void BM86xQwtPlot::onSetAuxVisible(const bool &value) {
    aux_show=value;

    if (aux_curve != nullptr) {
        aux_curve->setVisible(value);

        // Check Value Aux
        if (lastDmmData.value.a_mode == last_mode) {
            this->enableAxis(QwtPlot::yRight, false);
        } else {
            this->enableAxis(QwtPlot::yRight, aux_show);
        }
        onFilterDataChanged(mLastFilterData);
    }
}

void BM86xQwtPlot::onSetMainVisible(const bool &value) {
    main_show=value;

    if (main_curve != nullptr) {
        main_curve->setVisible(value);
        this->enableAxis(QwtPlot::yLeft, value);
        onFilterDataChanged(mLastFilterData);
    }
}

void BM86xQwtPlot::onSetPause(const bool &value) {
    m_pauseStatus = value;

    if (m_pauseStatus) {
        main_plot_data_bkp = main_plot_data;
        aux_plot_data_bkp = aux_plot_data;

        main_curve->setSamples(main_plot_data_bkp);
        aux_curve->setSamples(aux_plot_data_bkp);
    }
    else {
        main_curve->setSamples(main_plot_data);
        aux_curve->setSamples(aux_plot_data);
    }
}

/* ==============================================================================
 * Public function
 * ==============================================================================
 */
int BM86xQwtPlot::findNearestIndexX(const QVector<QPointF> &data, double targetX) const
{
    if (data.isEmpty()) return -1;

    int bestIndex = 0;
    double minDiff = qAbs(data.at(0).x() - targetX);

    // Iterate through the vector to find the smallest difference
    for (int i = 1; i < data.size(); ++i) {
        double diff = qAbs(data.at(i).x() - targetX);
        if (diff < minDiff) {
            minDiff = diff;
            bestIndex = i;
        }
    }
    return bestIndex;
}

void BM86xQwtPlot::setAntialiasing(const bool &value, bool replot)
{
    m_antialiasing = value;

    if (m_antialiasing) {
        main_curve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
        aux_curve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }
    else {
        main_curve->setRenderHint(QwtPlotItem::RenderAntialiased, false);
        aux_curve->setRenderHint(QwtPlotItem::RenderAntialiased, false);
    }

    if (replot)
        this->replot();
}

void BM86xQwtPlot::setColorXAxis(const QColor &color, bool replot) {
    if (color.isValid()) {
        mColorXAxis = color;

        int r, g, b;
        mColorXAxis.getRgb(&r, &g, &b);

        setStyleSheet(QString("border: none;color: rgb(%1, %2, %3);").arg(r).arg(g).arg(b));

        setCurveColor(mMainColor, mAuxColor, replot);
    }
}

void BM86xQwtPlot::setCurveColor(const QColor &main_color, const QColor &aux_color, bool replot)
{
    if (!main_color.isValid() || !aux_color.isValid())
        return;

    // MAIN
    mMainColor = main_color;
    main_pen.setColor(main_color);
    main_curve->setPen(main_pen);


    main_palette = axisWidget(QwtPlot::yLeft)->palette();
    main_palette.setColor(QPalette::WindowText, mMainColor);
    main_palette.setColor(QPalette::Text, mMainColor);
    axisWidget(QwtPlot::yLeft)->setPalette(main_palette);

    // AUX
    mAuxColor = aux_color;
    aux_pen.setColor(aux_color);
    if (main_color == aux_color) {
        aux_pen.setStyle(Qt::DashLine);
    }
    else {
        aux_pen.setStyle(Qt::SolidLine);
    }
    aux_curve->setPen(aux_pen);


    aux_palette = axisWidget(QwtPlot::yRight)->palette();
    aux_palette.setColor(QPalette::WindowText, mAuxColor);
    aux_palette.setColor(QPalette::Text, mAuxColor);
    axisWidget(QwtPlot::yRight)->setPalette(aux_palette);

    updateLegend();
    this->setPlotType(m_plotType, replot);
}

void BM86xQwtPlot::setMousePosColor(const QColor &color, bool replot)
{
    if (!color.isValid())
        return;

    mMousePosColor = color;

    if (m_isMouseOverPlot && m_lastMousePos != QPoint(-1, -1)) {
        double x = canvasMap(QwtPlot::xBottom).invTransform(m_lastMousePos.x());
        double y = canvasMap(QwtPlot::yLeft).invTransform(m_lastMousePos.y());
        dt.setMSecsSinceEpoch((int)x);

        // Update the status label with coordinates
        m_positionLabelText = QString("X=%1, Y=%2").
                              arg(dt.toString("hh:mm:ss.z")).
                              arg(y, 0, 'f', 2);
        m_positionLabelText.setRenderFlags( Qt::AlignLeft | Qt::AlignBottom );
        m_positionLabelText.setBackgroundBrush(QBrush( Qt::transparent ));
        m_positionLabelText.setColor( mMousePosColor );
        m_positionLabelText.setFont(m_positionFont);
        m_positionLabel.setText(m_positionLabelText);
        if (replot)
            this->replot();
    }
}

void BM86xQwtPlot::setPlotScale(const int &scale, bool replot)
{
    if (scale < BM86xPlot::last_scale)
        m_plotScale = scale;
    else
        return;

    if (main_plot_data.isEmpty())
        goto AUX;
    if (m_plotScale > BM86xPlot::ScaleFull) {
        qint64 index = findMinIndex(main_plot_data, main_plot_data.last().x() - m_plotScale);
        if (index < 0)
            index = 0;
        QVector<QPointF> temp_data = main_plot_data.mid(index);
        main_curve->setSamples(temp_data);
        this->setAxisScale(QwtPlot::xBottom, main_plot_data[index].x(), main_plot_data.last().x());
    }
    else {
        main_curve->setSamples(main_plot_data);
        this->setAxisScale(QwtPlot::xBottom, main_plot_data[0].x(), main_plot_data.last().x());
    }

AUX:
    if (aux_plot_data.isEmpty()) {
        if (main_plot_data.isEmpty()) {
            return;
        }
        else {
            goto END;
        }
    }

    if (m_plotScale > BM86xPlot::ScaleFull) {
        qint64 index = findMinIndex(aux_plot_data, aux_plot_data.last().x() - m_plotScale);
        if (index < 0)
            index = 0;
        QVector<QPointF> temp_data = aux_plot_data.mid(index);
        aux_curve->setSamples(temp_data);
        this->setAxisScale(QwtPlot::xBottom, aux_plot_data[index].x(), aux_plot_data.last().x());
    }
    else {
        aux_curve->setSamples(aux_plot_data);
        this->setAxisScale(QwtPlot::xBottom, aux_plot_data[0].x(), aux_plot_data.last().x());
    }

END:
    zoomer->setZoomBase();
    if (replot)
        this->replot();
}

void BM86xQwtPlot::setPlotType(const int &type, bool replot)
{
    if (type < BM86xPlot::PLOT_UNKNOWN)
    {
        m_plotType = type;
        static const int symbSize = 4;

        if (m_plotType < BM86xPlot::PLOT_SCATTER) {
            main_curve->setStyle( QwtPlotCurve::Lines );
        } else {
            main_curve->setStyle( QwtPlotCurve::NoCurve );
        }

        if (m_plotType == BM86xPlot::PLOT_CURVE) {
            main_curve->setCurveAttribute( QwtPlotCurve::Fitted );
        }
        else {
            main_curve->setCurveAttribute(QwtPlotCurve::Fitted, false);
        }

        if (m_plotType == BM86xPlot::PLOT_SCATTER) {
            main_curve->setSymbol( new QwtSymbol( QwtSymbol::Ellipse, mMainColor,
                                                QPen( main_pen ), QSize( symbSize, symbSize ) ) );
        }
        else {
            main_curve->setSymbol( new QwtSymbol( QwtSymbol::Ellipse, mMainColor,
                                                QPen( main_pen ), QSize( symbSize/2, symbSize/2 ) ) );
        }

        if (m_plotType < BM86xPlot::PLOT_SCATTER) {
            aux_curve->setStyle( QwtPlotCurve::Lines );
        } else {
            aux_curve->setStyle( QwtPlotCurve::NoCurve );
        }

        if (m_plotType == BM86xPlot::PLOT_CURVE) {
            aux_curve->setCurveAttribute( QwtPlotCurve::Fitted );
        }
        else {
            aux_curve->setCurveAttribute(QwtPlotCurve::Fitted, false);
        }

        if (m_plotType == BM86xPlot::PLOT_SCATTER) {
            aux_curve->setSymbol( new QwtSymbol( QwtSymbol::Diamond, mAuxColor,
                                               QPen( aux_pen ), QSize( symbSize, symbSize ) ) );
        }
        else {
            aux_curve->setSymbol( new QwtSymbol( QwtSymbol::Diamond, mAuxColor,
                                               QPen( aux_pen ), QSize( symbSize/2, symbSize/2 ) ) );
        }

        updateLegend();
        if (replot)
            this->replot();
    }
}

void BM86xQwtPlot::setBlackAndWhite() {
    static const int symbSize = 4;
    mMainBackupColor = mMainColor;
    mAuxBackupColor = mAuxColor;
    mBackupColorXAxis = mColorXAxis;
    mBackupMousePosColor = mMousePosColor;

    if (!mLegend)
    {
        mLegend = new QwtLegend;
        mLegend->setDefaultItemMode(QwtLegendData::Clickable); // ou ReadOnly
        mLegend->setWindowTitle("Plot Legend");
        connect(
            this,
            SIGNAL(legendDataChanged(const QVariant&,const QList<QwtLegendData>&)),
            mLegend,
            SLOT(updateLegend(const QVariant&,const QList<QwtLegendData>&)) );
    }

    if ( legend() )
    {
        // remove legend controlled by the plot
        insertLegend( NULL );
    }
    insertLegend(mLegend, QwtPlot::BottomLegend);

    if (mLegend)
        mLegend->show();

    // Change curve color to black
    setCurveColor(QColor(Qt::black), QColor(Qt::black), false);

    // Change X axis color to black
    setColorXAxis(QColor(Qt::black), false);

    // Change mouse coordinates color to black
    setMousePosColor(QColor(Qt::black), false);

    if (m_plotType < BM86xPlot::PLOT_SCATTER) {
        main_curve->setSymbol( new QwtSymbol( QwtSymbol::Ellipse, mMainColor,
                                            QPen( main_pen ), QSize( symbSize/2, symbSize/2 ) ) );
        aux_curve->setSymbol( new QwtSymbol( QwtSymbol::Diamond, mAuxColor,
                                           QPen( aux_pen ), QSize( symbSize/2, symbSize/2 ) ) );

        // Set legend
        main_curve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
        main_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, true);

        aux_curve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
        aux_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, true);
    }
    else {
        main_curve->setSymbol( new QwtSymbol( QwtSymbol::Ellipse, mMainColor,
                                            QPen( main_pen ), QSize( symbSize, symbSize ) ) );
        aux_curve->setSymbol( new QwtSymbol( QwtSymbol::Diamond, mAuxColor,
                                           QPen( aux_pen ), QSize( symbSize, symbSize ) ) );

        // Set legend
        main_curve->setLegendAttribute(QwtPlotCurve::LegendShowLine, false);
        main_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, true);

        aux_curve->setLegendAttribute(QwtPlotCurve::LegendShowLine, false);
        aux_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, true);
    }

    main_curve->itemChanged();
    aux_curve->itemChanged();

    updateLegend();

    // Hide filter
    if (m_mainZoneCenter) m_mainZoneCenter->hide();
    if (m_mainZoneOutsideLow) m_mainZoneOutsideLow->hide();
    if (m_mainZoneOutsideHigh) m_mainZoneOutsideHigh->hide();
    if (m_auxZoneCenter) m_auxZoneCenter->hide();
    if (m_auxZoneOutsideLow) m_auxZoneOutsideLow->hide();
    if (m_auxZoneOutsideHigh) m_auxZoneOutsideHigh->hide();

    // Hide mouse
    m_positionLabel.setText(QString(""));

    // Change axis title
    //Set Main Unit
    QString mainTitle = "Main\n";
    if (lastDmmData.value.m_mode == Res || lastDmmData.value.m_mode == Cont) {
        if (lastDmmData.value.m_unit == ohm_unit) {
            mainTitle.append("\u03A9");
        }
        else if (lastDmmData.value.m_unit == Kohm_unit) {
            mainTitle.append("k\u03A9");
        }
        else {
            mainTitle.append("M\u03A9");
        }
    }
    else {
        mainTitle.append(lastDmmData.value.m_unitString);
    }
    this->setAxisTitle(QwtPlot::yLeft,mainTitle);

    //Set Aux Unit
    QString auxTitle = "Aux\n";
    if (lastDmmData.value.a_mode == Res || lastDmmData.value.a_mode == Cont) {
        if (lastDmmData.value.m_unit == ohm_unit) {
            auxTitle.append("\u03A9");
        }
        else if (lastDmmData.value.m_unit == Kohm_unit) {
            auxTitle.append("k\u03A9");
        }
        else {
            auxTitle.append("M\u03A9");
        }
    }
    else {
        auxTitle.append(lastDmmData.value.a_unitString);
    }
    this->setAxisTitle(QwtPlot::yRight,auxTitle);

    replot();
}

void BM86xQwtPlot::restoreColor() {
    setCurveColor(mMainBackupColor, mAuxBackupColor, false);
    setColorXAxis(mBackupColorXAxis, false);
    setPlotType(m_plotType, false);
    setMousePosColor(mBackupMousePosColor, false);

    if ( legend() )
    {
        // remove legend controlled by the plot
        insertLegend( NULL );
    }

    // Reset axis title
    //Set Main Unit
    if (lastDmmData.value.m_mode == Res || lastDmmData.value.m_mode == Cont) {
        if (lastDmmData.value.m_unit == ohm_unit) {
            this->setAxisTitle(QwtPlot::yLeft,"\u03A9");
        }
        else if (lastDmmData.value.m_unit == Kohm_unit) {
            this->setAxisTitle(QwtPlot::yLeft,"k\u03A9");
        }
        else {
            this->setAxisTitle(QwtPlot::yLeft,"M\u03A9");
        }
    }
    else {
        this->setAxisTitle(QwtPlot::yLeft,lastDmmData.value.m_unitString);
    }

    //Set Aux Unit
    if (lastDmmData.value.a_mode == Res || lastDmmData.value.a_mode == Cont) {
        if (lastDmmData.value.a_unit == ohm_unit) {
            this->setAxisTitle(QwtPlot::yRight,"\u03A9");
        }
        else if (lastDmmData.value.a_unit == Kohm_unit) {
            this->setAxisTitle(QwtPlot::yRight,"k\u03A9");
        }
        else {
            this->setAxisTitle(QwtPlot::yRight,"M\u03A9");
        }
    }
    else {
        this->setAxisTitle(QwtPlot::yRight,lastDmmData.value.a_unitString);
    }

    onFilterDataChanged(mLastFilterData); // This do the replot()
}

#include <algorithm> // For std::lower_bound
// Returns the smallest index for which list[index].x() >= val.
// The list must be sorted by x() in ascending order.
qint64 BM86xQwtPlot::findMinIndex(const QList<QPointF> &list, qint64 val)
{
    if (list.isEmpty()) return -1;

    // Looking for the iterator pointing to the first element where x() is >= val
    auto it = std::lower_bound(list.begin(), list.end(), static_cast<double>(val),
                               [](const QPointF& element, double value) {
                                   return element.x() < value;
                               });

    if (it != list.end()) {
        return std::distance(list.begin(), it);
    }

    return -1;
}

// qint64 BM86xQwtPlot::findMinIndex(const QList<QPointF> &list, qint64 val)
// {
//     const qsizetype size = list.size();

//     if (size == 0)
//         return -1;

//     const QPointF *data = list.constData();

//     qsizetype first = 0;
//     qsizetype last = size;

//     while (first < last) {
//         const qsizetype middle = first + (last - first) / 2;

//         if (data[middle].x() < val)
//             first = middle + 1;
//         else
//             last = middle;
//     }

//     return first < size ? first : -1;
// }

/* ==============================================================================
 * Private function
 * ==============================================================================
 */
void BM86xQwtPlot::init() {
    this->setAxisTitle(QwtPlot::xBottom,"time\n[hh:mm:ss.z]");
    this->setAxisScaleDraw( QwtPlot::xBottom,
                           new TimeScaleDraw() );
    QwtScaleWidget *scaleWidget = this->axisWidget( QwtPlot::xBottom );
    const int fmh = QFontMetrics( scaleWidget->font() ).height();
    scaleWidget->setMinBorderDist( 0, fmh / 2 );

    //MAIN
    if (!main_curve) {
        main_curve = new QwtPlotCurve("Main");
        main_curve->setAxes(QwtPlot::xBottom, QwtPlot::yLeft);
        main_curve->attach(this);
        main_curve->setRenderThreadCount(0);
        main_curve->setSamples(main_plot_data);
        main_curve->setLegendIconSize( QSize(6, 6) );
    }

    //AUX
    if (!aux_curve) {
        aux_curve = new QwtPlotCurve("Aux");
        aux_curve->setAxes(QwtPlot::xBottom, QwtPlot::yRight);
        aux_curve->attach(this);
        aux_curve->setRenderThreadCount(0);
        aux_curve->setSamples(aux_plot_data);
        aux_curve->setLegendIconSize( QSize(6, 6) );
    }

    this->setAxisAutoScale(QwtPlot::yLeft);
    this->setAxisAutoScale(QwtPlot::yRight);
    this->setAxisAutoScale(QwtPlot::xBottom);

    this->setCurveColor(mMainColor, mAuxColor, false);
    this->setAntialiasing(m_antialiasing, false);
    this->setPlotType(m_plotType);
}

BM86xQwtPlot::AxisBoundary BM86xQwtPlot::getBoundaryY(const QVector<QPointF> &data) const {
    AxisBoundary result = {data.first().y(),data.first().y()};
    for (auto& point : data) {
        result.min = ( qMin(result.min, point.y()) );
        result.max = ( qMax(result.max, point.y()) );
    }
    return result;
}

BM86xQwtPlot::AxisBoundary BM86xQwtPlot::getBoundaryX(const QVector<QPointF> &data) const {
    AxisBoundary result = {data.first().x(),data.first().x()};
    for (auto& point : data) {
        result.min = ( qMin(result.min, point.x()) );
        result.max = ( qMax(result.max, point.x()) );
    }
    return result;
}

BM86xQwtPlot::PlotBoundary BM86xQwtPlot::getPlotBoundary() const {
    PlotBoundary boundary;
    boundary.xBottom.max = this->axisScaleDiv(QwtPlot::xBottom).upperBound();
    boundary.xBottom.min = this->axisScaleDiv(QwtPlot::xBottom).lowerBound();
    boundary.yLeft.max   = this->axisScaleDiv(QwtPlot::yLeft).upperBound();
    boundary.yLeft.min   = this->axisScaleDiv(QwtPlot::yLeft).lowerBound();
    boundary.yRight.max  = this->axisScaleDiv(QwtPlot::yRight).upperBound();
    boundary.yRight.min  = this->axisScaleDiv(QwtPlot::yRight).lowerBound();

    return boundary;
}

void BM86xQwtPlot::savePlotBoundary() {
    mPlotBoundary = getPlotBoundary();
}

void BM86xQwtPlot::setPlotBoundary (const BM86xQwtPlot::PlotBoundary& boundary) {
    this->setAxisScale(QwtPlot::xBottom, boundary.xBottom.max, boundary.xBottom.min);
    this->setAxisScale(QwtPlot::yLeft, boundary.yLeft.max, boundary.yLeft.min);
    this->setAxisScale(QwtPlot::yRight, boundary.yRight.max, boundary.yRight.min);
}

void BM86xQwtPlot::scrollX(int steps) {
    const double min = axisScaleDiv(QwtPlot::xBottom).lowerBound();
    const double max = axisScaleDiv(QwtPlot::xBottom).upperBound();

    const double width = max - min;
    const double delta = width * 0.2 * steps;

    setAxisScale(QwtPlot::xBottom,
                 min + delta,
                 max + delta);

    replot();
}

/* ==============================================================================
 * Protected function
 * ==============================================================================
 */
// Left drag             : Zoom in
// Right click           : Zoom out one level
// Ctrl + Right click    : Reset zoom
// Middle drag            : Pan the plot
// Wheel                  : Zoom Y axis
// Shift + Wheel          : Zoom X axis
// Ctrl + Wheel           : Zoom X + Y axes
// Alt + Wheel            : Scroll X axis (history)
bool BM86xQwtPlot::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == this->canvas()) {
        if (event->type() == QEvent::MouseMove || event->type() == QEvent::Paint) {
            if (m_isMouseOverPlot && m_lastMousePos != QPoint(-1, -1)) {
                QMouseEvent *mouseEvent = nullptr;
                QPointF canvasPos;
                QColor displayColor = mMousePosColor;

                if (event->type() == QEvent::MouseMove) {
                    mouseEvent = static_cast<QMouseEvent *>(event);
                    canvasPos = mouseEvent->pos();
                } else {
                    canvasPos = m_lastMousePos;
                }

                // Reset snap state
                m_isSnapping = 0;
                QPointF finalCanvasPos = canvasPos; // Default position (no snap)
                int snappedIndex = -1;
                QwtPlotCurve *snappedCurve = nullptr;

                // 1. Get the mouse's X plot position
                double mousePlotX = canvasMap(QwtPlot::xBottom).invTransform(canvasPos.x());

                // --- Check on MAIN ---
                if (!main_plot_data.isEmpty() && main_show == true) {
                    int idx = findNearestIndexX(main_plot_data, mousePlotX);
                    if (idx >= 0) {
                        // Point coordinates in the plot coordinate system
                        QPointF plotPoint = main_plot_data[idx];

                        // Convert to pixels (canvas coordinates)
                        QPointF canvasPoint = QPointF(canvasMap(QwtPlot::xBottom).transform(plotPoint.x()),
                                                      canvasMap(QwtPlot::yLeft).transform(plotPoint.y()));

                        // Euclidean distance between the mouse and the point
                        double dist = (canvasPoint - canvasPos).manhattanLength();

                        // If the distance is below the threshold, candidate this point
                        if (dist < m_snapThreshold) {
                            m_isSnapping = 1;
                            displayColor = mMainColor;
                            finalCanvasPos = canvasPoint;
                            snappedIndex = idx;
                            snappedCurve = main_curve; // Assuming main_curve is linked to main_plot_data
                        }
                    }
                }

                // --- Check on AUX (Only if not already snapped or if AUX is preferred) ---
                // Here, we keep the principle: the first found (or the closest if comparing distances).
                // For simplicity, if we have already snapped to MAIN, we do not snap to AUX unless AUX is closer.

                if (!aux_plot_data.isEmpty() && aux_show == true) {
                    int idx = findNearestIndexX(aux_plot_data, mousePlotX);
                    if (idx >= 0) {
                        QPointF plotPoint = aux_plot_data[idx];
                        QPointF canvasPoint = QPointF(canvasMap(QwtPlot::xBottom).transform(plotPoint.x()),
                                                      canvasMap(QwtPlot::yRight).transform(plotPoint.y()));

                        double dist = (canvasPoint - canvasPos).manhattanLength();

                        // Compare with the best distance found so far
                        // If we haven't snapped yet (dist > threshold) or if this curve is closer
                        if (dist < m_snapThreshold) {
                            // If we already have a candidate snap, keep the closest one
                            if (snappedCurve == nullptr || !m_isSnapping || dist < (QPointF(canvasMap(QwtPlot::xBottom).transform(snappedCurve->sample(snappedIndex).x()),
                                                                                            canvasMap(QwtPlot::yRight).transform(snappedCurve->sample(snappedIndex).y())) - canvasPos).manhattanLength()) {
                                m_isSnapping = 2;
                                displayColor = mAuxColor;
                                finalCanvasPos = canvasPoint;
                            }
                        }
                    }
                }

                // --- Update final position ---
                // If m_isSnapping is true, use finalCanvasPos, otherwise use the original canvasPos
                m_lastMousePos = m_isSnapping ? finalCanvasPos : canvasPos;

                // --- Display text ---
                double y;
                double x = canvasMap(QwtPlot::xBottom).invTransform(m_lastMousePos.x());
                if (m_isSnapping > 1) {
                    y = canvasMap(QwtPlot::yRight).invTransform(m_lastMousePos.y());
                }
                else {
                    y = canvasMap(QwtPlot::yLeft).invTransform(m_lastMousePos.y());
                }


                dt.setMSecsSinceEpoch((int)x);

                m_positionLabelText = QString("X=%1, Y=%2").
                                      arg(dt.toString("hh:mm:ss.z")).
                                      arg(y, 0, 'f', 2);

                m_positionLabelText.setRenderFlags(Qt::AlignLeft | Qt::AlignBottom);
                m_positionLabelText.setBackgroundBrush(QBrush(Qt::transparent));
                m_positionLabelText.setColor(displayColor);
                m_positionLabelText.setFont(m_positionFont);
                m_positionLabel.setText(m_positionLabelText);

                this->replot();
            }
        }
        else if (event->type() == QEvent::Leave) {
            m_isMouseOverPlot = false;
            m_positionLabel.setText(QString("")); // Clear the status label when the mouse leaves
            this->replot();
        }
        else if (event->type() == QEvent::Enter) {
            m_isMouseOverPlot = true;
        }
        else if (event->type() == QEvent::Wheel) {
            auto *wheelEvent = static_cast<QWheelEvent *>(event);

            requestPause();

            if (wheelEvent->modifiers() & Qt::AltModifier)
            {
                const int steps = wheelEvent->angleDelta().y() / 120;

                if (steps != 0)
                {
                    scrollX(steps);
                    return true;
                }
            }
        }
        else if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);

            // Left drag / Right click / Middle drag
            if (mouseEvent->button() == Qt::LeftButton ||
                mouseEvent->button() == Qt::RightButton ||
                mouseEvent->button() == Qt::MiddleButton)
            {
                // if (mouseEvent->modifiers() & Qt::ControlModifier)
                //     requestPlay();
                // else
                    requestPause();
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
