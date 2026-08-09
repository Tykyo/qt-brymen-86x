/**
 * @file shortcutaction.h
 *
 * Copyright (c) 2026 Olivier Verlaine
 * SPDX-License-Identifier: MIT
 */

#ifndef SHORTCUTACTION_H
#define SHORTCUTACTION_H

#include <QtWidgets/qpushbutton.h>
#include <QPointer>
#include <QAction>
#include <QKeySequence>
#include <QString>
#include <QColor>

#pragma once
namespace Shortcut
{
    struct Action
    {
        QString text;
        QString toolTip;
        QPointer<QAction> action = nullptr;
        QKeySequence shortcut = QKeySequence::UnknownKey;

        void apply()
        {
            if (!action)
                return;

            if (shortcut != QKeySequence::UnknownKey)
                action->setShortcut(shortcut);

            action->setText(text);
            action->setToolTip(toolTip);
        }

        void setShortcut(const QKeySequence &sc)
        {
            shortcut = sc;

            if (action)
                action->setShortcut(shortcut);
        }
    };

    struct ButtonParam
    {
        QPointer<QPushButton> button = nullptr;
        QPointer<QAction>     action = nullptr;
    };
}

#endif // SHORTCUTACTION_H
