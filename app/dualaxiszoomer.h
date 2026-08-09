/**
 * @file dualaxiszoomer.h
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#ifndef DUALAXISZOOMER_H
#define DUALAXISZOOMER_H

#include <qwt_plot.h>
#include <qwt_plot_zoomer.h>
#include <qwt_scale_div.h>
#include <qwt_scale_map.h>
#include <QStack>

/*!
    \class DualAxisZoomer
    \brief Extends QwtPlotZoomer to synchronize secondary plot axes.

    QwtPlotZoomer updates only the axes it controls. When a plot has
    secondary X and/or Y axes with independent scales, they normally
    remain unchanged during zoom operations.

    DualAxisZoomer overrides QwtPlotZoomer::rescale() and updates the
    secondary axes using the current canvas transformation before the
    primary axes are rescaled. This preserves the visual alignment of
    all axes while keeping their value ranges independent.

    The class supports:
      - secondary X axis synchronization;
      - secondary Y axis synchronization;
      - different scales on primary and secondary axes;
      - zoom in, zoom out and zoom reset;
      - wheel zoom (QwtPlotMagnifier) and rectangle zoom
        (QwtPlotZoomer).

    The implementation does not modify Qwt itself and can therefore be
    used as a drop-in replacement for QwtPlotZoomer.
*/

class DualAxisZoomer : public QwtPlotZoomer
{
    Q_OBJECT
public:

    explicit DualAxisZoomer (QWidget *canvas, bool syncYAxisEnable = false, bool syncXAxisEnable = false)
        : QwtPlotZoomer(canvas),
        m_syncXAxisEnable(syncXAxisEnable),
        m_syncYAxisEnable(syncYAxisEnable)
    {
        if (yAxis() == QwtPlot::yRight) {
            m_secondaryYAxis = QwtPlot::yLeft;
        }
        else {
            m_secondaryYAxis = QwtPlot::yRight;
        }

        if (xAxis() == QwtPlot::xBottom) {
            m_secondaryXAxis = QwtPlot::xTop;
        }
        else {
            m_secondaryXAxis = QwtPlot::xBottom;
        }
    }

    int syncXAxis() const
    {
        return m_secondaryXAxis;
    }

    int syncYAxis() const
    {
        return m_secondaryYAxis;
    }

    void setSyncXAxisEnabled (bool value)
    {
        m_syncXAxisEnable = value;
    }

    void setSyncYAxisEnabled (bool value)
    {
        m_syncYAxisEnable = value;
    }

    void setAxes( QwtAxisId xAxis, QwtAxisId yAxis ) override
    {
        if (yAxis == QwtPlot::yRight) {
            m_secondaryYAxis = QwtPlot::yLeft;
        }
        else {
            m_secondaryYAxis = QwtPlot::yRight;
        }

        if (xAxis == QwtPlot::xBottom) {
            m_secondaryXAxis = QwtPlot::xTop;
        }
        else {
            m_secondaryXAxis = QwtPlot::xBottom;
        }

        QwtPlotZoomer::setAxes(xAxis, yAxis);
    }

protected:
    /*
     * Synchronize secondary axes before calling QwtPlotZoomer::rescale().
     *
     * At this point the canvas maps still represent the previous view,
     * allowing the current zoom rectangle to be converted into the
     * corresponding ranges of the secondary axes.
     *
     * Once QwtPlotZoomer::rescale() has been executed, this information
     * is no longer available.
     */
    void rescale() override {
        QwtPlot* plt = plot();
        if ( !plt )
            return;

        /*
         * If the plot has already been rescaled (e.g. by a QwtPlotMagnifier)
         * before the first rectangle zoom, the current scale is not part of
         * the zoom stack. Insert it so that the zoom history reflects the
         * actual plot state.
         */
        if (scaleRect() != zoomBase() && zoomRectIndex() == 1 && m_previousIndex == 0) {
            QStack< QRectF> updatedZoomStack = zoomStack();
            updatedZoomStack.insert(zoomRectIndex(), scaleRect());
            setZoomStack(updatedZoomStack, updatedZoomStack.count() - 1);
        }

        const QRectF& rect = zoomStack().at( zoomRectIndex() );
        if ( rect != scaleRect() )
        {
            const QwtScaleMap masterXMap = plt->canvasMap(xAxis());
            const QwtScaleMap masterYMap = plt->canvasMap(yAxis());

            const QwtScaleMap slaveXMap = plt->canvasMap(m_secondaryXAxis);
            const QwtScaleMap slaveYMap = plt->canvasMap(m_secondaryYAxis);

            const bool doReplot = plt->autoReplot();
            plt->setAutoReplot( false );

            const double p1x = masterXMap.transform(rect.left());
            const double p2x = masterXMap.transform(rect.right());

            double x1 = slaveXMap.invTransform(p1x);
            double x2 = slaveXMap.invTransform(p2x);
            if ( !plt->axisScaleDiv( m_secondaryXAxis ).isIncreasing() )
                qSwap( x1, x2 );

            if (m_syncXAxisEnable)
                plt->setAxisScale( m_secondaryXAxis, x1, x2 );

            const double p1y = masterYMap.transform(rect.top());
            const double p2y = masterYMap.transform(rect.bottom());

            double y1 = slaveYMap.invTransform(p1y);
            double y2 = slaveYMap.invTransform(p2y);
            if ( !plt->axisScaleDiv( m_secondaryYAxis ).isIncreasing() )
                qSwap( y1, y2 );

            if (m_syncYAxisEnable)
                plt->setAxisScale( m_secondaryYAxis, y1, y2 );

            plt->setAutoReplot( doReplot );

            QwtPlotZoomer::rescale();
            m_previousIndex = zoomRectIndex();

#ifdef QT_DEBUG
            qDebug() << "      zoomStack() :" << QwtPlotZoomer::zoomStack();
            qDebug() << "  zoomRectIndex() :" << QwtPlotZoomer::zoomRectIndex();
            qDebug() << "        slaveXMap :" << slaveXMap;
            qDebug() << "        slaveYMap :" << slaveYMap;
            qDebug() << "       masterXMap :" << masterXMap;
            qDebug() << "       masterYMap :" << masterYMap;
            qDebug() << "             rect :" << rect;
            qDebug() << "              p1x :" << p1x;
            qDebug() << "              p2x :" << p2x;
            qDebug() << "              p1y :" << p1y;
            qDebug() << "              p2y :" << p2y;
            qDebug() << "               y1 :" << y1;
            qDebug() << "               y2 :" << y2;
            qDebug() << "";
#endif
        }
    }

private:
    bool m_syncXAxisEnable;
    bool m_syncYAxisEnable;
    int  m_secondaryXAxis;
    int  m_secondaryYAxis;
    int  m_previousIndex = 0;
};

#endif
