/**
 * @file main.cpp
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#include "bm86xgui.h"

#include <QApplication>
#include <QStyleFactory>
#include <QStyleHints>
#include <QCoreApplication>
#include <stdio.h>
#include <stdlib.h>

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug   : %s -> %s\n", QDateTime::currentDateTime().toString("hh:mm:ss.zzz").toStdString().c_str(), localMsg.constData());
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info    : %s -> %s\n", QDateTime::currentDateTime().toString("hh:mm:ss.zzz").toStdString().c_str(), localMsg.constData());
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning : %s -> %s (%s:%u, %s)\n", QDateTime::currentDateTime().toString("hh:mm:ss.zzz").toStdString().c_str(), localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s -> %s (%s:%u, %s)\n", QDateTime::currentDateTime().toString("hh:mm:ss.zzz").toStdString().c_str(), localMsg.constData(), file, context.line, function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal   : %s -> %s (%s:%u, %s)\n", QDateTime::currentDateTime().toString("hh:mm:ss.zzz").toStdString().c_str(), localMsg.constData(), file, context.line, function);
        break;
    }
}


int main(int argc, char *argv[])
{
    qInstallMessageHandler(myMessageOutput);
    QApplication a(argc, argv);

    // CRITICAL: Set organization and application names.
    // This allows QStandardPaths to build the correct path automatically.
    // Windows: AppData/Local/Organization/ApplicationName
    // macOS: ~/Library/Application Support/Organization/ApplicationName
    // Linux: ~/.local/share/Organization/ApplicationName
    QCoreApplication::setOrganizationName("Tykyo");  // Your company/author name
    QCoreApplication::setApplicationName(QString(_TARGET));   // Your application name

    a.setStyle(QStyleFactory::create("Fusion"));
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);

    BM86Xgui w;
    w.show();
    return QApplication::exec();
}
