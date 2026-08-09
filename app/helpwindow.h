/**
 * @file helpwindow.h
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */
#ifndef HELPWINDOW_H
#define HELPWINDOW_H

#include <QDialog>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QPointer>
#include <QtCore/qdir.h>

class HelpWindow : public QDialog {
    Q_OBJECT

public:
    explicit HelpWindow(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Help - Documentation");
        resize(600, 400);

        QPointer<QTextBrowser> helpViewer = new QTextBrowser(this);

        helpViewer->setSearchPaths(QStringList() << ":/docs");

        QFile file(":/docs/help.md");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            helpViewer->setMarkdown(in.readAll());
        }

        helpViewer->setOpenExternalLinks(true);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(helpViewer);
        setLayout(layout);
    }
};

#endif // HELPWINDOW_H
