/**
 * @file bm86xqwtplot.h
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#ifndef BM86XQWTPLOT_H
#define BM86XQWTPLOT_H

#include <QEvent>
#include <QObject>
#include <QTimeZone>
#include <QWidget>
#include <QtGui/qevent.h>
#include <qwt_picker_machine.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_magnifier.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_picker.h>
#include <qwt_plot_renderer.h>
#include <qwt_plot_textlabel.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_map.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>
#include <qwt_text.h>
#include <qwt_global.h>
#include <qwt_plot_scaleitem.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_item.h>
#include <qwt_plot_zoneitem.h>
#include <qwt_legend.h>
#include "dualaxiszoomer.h"

#include "datastorage.h"
#include "bm86xplot.h"

//Customize the display time scale of the y-axis
class TimeScaleDraw: public QwtScaleDraw
{
public:
    TimeScaleDraw()
    {
    }
    QwtText label( double v ) const
    {
        static QDateTime dt;
        static QTimeZone tz = QTimeZone::UTC;
        dt.setTimeZone(tz);
        dt.setMSecsSinceEpoch((qint64)v);

        if (dt.time().hour() == 0)
            if (v >= 60000) {
                return dt.toString("mm:ss");
            }
            else {
                return dt.toString("ss.z");
            }
        else
            return dt.toString("hh:mm:ss");
    }
};
class DistancePicker: public QwtPlotPicker
{
public:
    DistancePicker( QWidget *canvas ):
        QwtPlotPicker( canvas )
    {
        setTrackerMode( QwtPicker::ActiveOnly );
        setStateMachine( new QwtPickerDragLineMachine() );
        setRubberBand( QwtPlotPicker::PolygonRubberBand );
    }

    virtual QwtText trackerTextF( const QPointF &pos ) const
    {
        QwtText text;

        const QPolygon points = selection();
        if ( !points.isEmpty() )
        {
            QString num;
            num.setNum( QLineF( pos, invTransform( points[0] ) ).length() );

            QColor bg( Qt::white );
            bg.setAlpha( 200 );

            text.setBackgroundBrush( QBrush( bg ) );
            text.setText( num );
        }
        return text;
    }
};

class BM86xQwtPlot : public QwtPlot
{
    Q_OBJECT

public:
    explicit BM86xQwtPlot( QWidget* = NULL );

    int  findNearestIndexX (const QVector<QPointF> &data, double targetX) const;
    void setAntialiasing (const bool &value, bool replot = true);
    void setColorXAxis (const QColor& color, bool replot = true);
    void setCurveColor (const QColor &main_color, const QColor &aux_color, bool replot = true);
    void setMousePosColor (const QColor &color, bool replot = true);
    void setPlotScale (const int &scale, bool replot = true);
    void setPlotType (const int &type, bool replot = true);
    void setBlackAndWhite ();
    void restoreColor ();

    int  getPlotScale   () const {return m_plotScale;}
    int  getPlotType    () const {return m_plotType;}
    bool isAntialiasing () const {return m_antialiasing;}
    bool isMainVisible  () const {return main_show;}
    bool isAuxVisible   () const {return aux_show;}

public Q_SLOTS:
    void onAppendData (const BM86xDataType_s &data);
    void onClearData (void);
    void onFilterDataChanged (const DataStorage::filterData_s &value);
    void onSetAuxVisible (const bool &value);
    void onSetMainVisible (const bool &value);
    void onSetPause (const bool &value);

private:
    struct AxisBoundary
    {
        double min;
        double max;
    };

    struct PlotBoundary
    {
        AxisBoundary xBottom;
        AxisBoundary yLeft;
        AxisBoundary yRight;
    };

    int m_plotType = BM86xPlot::PLOT_LINE;
    int m_plotScale = BM86xPlot::ScaleFull;
    bool m_antialiasing = true;
    bool m_pauseStatus = false;
    bool main_show = true;
    bool aux_show = true;
    bool m_isMouseOverPlot = false;
    QPointF m_lastMousePos;
    QVector<QPointF> main_plot_data;
    QVector<QPointF> aux_plot_data;
    QVector<QPointF> main_plot_data_bkp;
    QVector<QPointF> aux_plot_data_bkp;

    QColor mMainColor           = QColor(255, 204, 0);
    QColor mAuxColor            = QColor(0, 255, 255);
    QColor mMainBackupColor     = QColor(255, 204, 0);
    QColor mAuxBackupColor      = QColor(0, 255, 255);
    QColor mColorXAxis          = QColor(125, 125, 125);
    QColor mBackupColorXAxis    = QColor(125, 125, 125);
    QColor mMousePosColor       = QColor(125,125,125);
    QColor mBackupMousePosColor = QColor(125,125,125);

    QPen main_pen;
    QPen aux_pen;
    QPalette main_palette;
    QPalette aux_palette;
    QDateTime time;

    QwtPlotCurve     *main_curve            = nullptr;
    QwtPlotCurve     *aux_curve             = nullptr;
    QwtPlotZoneItem  *m_mainZoneCenter      = nullptr;
    QwtPlotZoneItem  *m_mainZoneOutsideLow  = nullptr;
    QwtPlotZoneItem  *m_mainZoneOutsideHigh = nullptr;
    QwtPlotZoneItem  *m_auxZoneCenter       = nullptr;
    QwtPlotZoneItem  *m_auxZoneOutsideLow   = nullptr;
    QwtPlotZoneItem  *m_auxZoneOutsideHigh  = nullptr;

    QPointer<QwtPlotPanner>    panner       = nullptr;
    QPointer<QwtPlotMagnifier> magnifierY   = nullptr;
    QPointer<QwtPlotMagnifier> magnifierX   = nullptr;
    QPointer<QwtPlotMagnifier> magnifierXY  = nullptr;
    QPointer<QwtLegend>        mLegend      = nullptr;
    QPointer<DualAxisZoomer>   zoomer       = nullptr;

    QwtPlotTextLabel m_positionLabel;
    QwtText m_positionLabelText;
    QFont m_positionFont = {"Courier New", 12, QFont::Bold};
    QDateTime dt;
    QTimeZone tz = QTimeZone::UTC;
    BM86xDataType_s lastDmmData;
    PlotBoundary mPlotBoundary;
    DataStorage::filterData_s mLastFilterData;

    uint8_t m_isSnapping = 0;
    int m_snapThreshold = 10; // Snap radius in pixels (10px is a standard value)

    // Variables to store the snapped position
    QPointF m_snappedCanvasPos;
    int m_snappedIndex; // Index of the point on the snapped curve
    QwtPlotCurve *m_snappedCurve; // Pointer to the curve that was snapped

    BM86x_ModeTypeDef main_previousMode = last_mode;
    BM86x_UnitTypeDef main_previousUnit = last_unit;
    BM86x_ModeTypeDef aux_previousMode  = last_mode;
    BM86x_UnitTypeDef aux_previousUnit  = last_unit;
    BM86x_PeakModeTypeDef previousPeak  = last_peak_mode;

    void   init ();
    qint64 findMinIndex (const QList<QPointF> &list, qint64 val);
    AxisBoundary getBoundaryX (const QVector<QPointF> &data) const;
    AxisBoundary getBoundaryY (const QVector<QPointF> &data) const;
    PlotBoundary getPlotBoundary () const;
    void savePlotBoundary ();
    void setPlotBoundary (const PlotBoundary& boundary);
    void scrollX (int steps=1);

    void requestPause() { if (!m_pauseStatus) Q_EMIT(pausePlot()); }
    // void requestPlay() { if (m_pauseStatus) Q_EMIT(pausePlot()); }

Q_SIGNALS:
    void pausePlot ();

protected:
    bool eventFilter (QObject *obj, QEvent *event) override;
};

#endif // BM86XQWTPLOT_H
