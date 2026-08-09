/**
 * @file bm86xplot.h
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#ifndef BM86XPLOT_H
#define BM86XPLOT_H

#define MIN_TO_MSEC 60000

namespace BM86xPlot {

enum plotType {
    PLOT_CURVE,
    PLOT_LINE,
    PLOT_SCATTER,
    PLOT_UNKNOWN,

    last_plot
};

enum scale {
    ScaleFull  = 0 * MIN_TO_MSEC,
    Seconds_5  = MIN_TO_MSEC / 12,
    Seconds_10 = MIN_TO_MSEC / 6,
    Seconds_30 = MIN_TO_MSEC / 2,
    Minutes_1  = 1 * MIN_TO_MSEC,
    Minutes_5  = 5 * MIN_TO_MSEC,
    Minutes_10 = 10 * MIN_TO_MSEC,
    Minutes_30 = 30 * MIN_TO_MSEC,
    Minutes_60 = 60 * MIN_TO_MSEC,

    last_scale
};

}

#endif // BM86XPLOT_H
