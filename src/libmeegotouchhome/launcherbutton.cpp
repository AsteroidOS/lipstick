/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of mhome.
**
** If you have questions regarding the use of this file, please contact
** Nokia at directui@nokia.com.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include "launcherbutton.h"
#include "launcher.h"
#include <MDesktopEntry>
#include <QDBusInterface>

#include <MWidgetCreator>
M_REGISTER_WIDGET(LauncherButton)

#include "homewindowmonitor.h"

bool LauncherButton::launching = false;

LauncherButton::LauncherButton(const QString &desktopEntryPath, MWidget *parent) :
        MButton(parent, new LauncherButtonModel),
        windowMonitor(new HomeWindowMonitor)
{
    init();

    updateFromDesktopEntry(desktopEntryPath);
}

LauncherButton::~LauncherButton()
{
}

void LauncherButton::init()
{
    // When the button is clicked the related object should be launched
    connect(this, SIGNAL(clicked()), this, SLOT(launch()));
}

QString LauncherButton::desktopEntry() const
{
    return model()->desktopEntry()->fileName();
}

LauncherButtonModel::State LauncherButton::buttonState() const
{
    return model()->buttonState();
}

void LauncherButton::launch()
{
    if (!launching) {
        if (model()->buttonState() == LauncherButtonModel::Installed) {
            model()->setButtonState(LauncherButtonModel::Launching);

            connect(windowMonitor.data(), SIGNAL(fullscreenWindowOnTopOfOwnWindow()), SLOT(stopLaunchProgress()));

            launching = true;
            action.trigger();
        } else if (model()->buttonState() == LauncherButtonModel::Broken) {
            // Show the exception dialog for this package
            if (!model()->desktopEntry().isNull()) {
                QString package = model()->desktopEntry()->value("X-MeeGo", "Package");
                if (!package.isEmpty()) {
                    QDBusInterface interface("com.nokia.package_manager_install_ui",
                                           "/com/nokia/package_manager_install_ui",
                                           "com.nokia.package_manager_install_ui",
                                           QDBusConnection::sessionBus());
                    interface.call("show_installation_exception", package);
                }
            }
        }
    }
}

void LauncherButton::stopLaunchProgress()
{
    model()->setButtonState(LauncherButtonModel::Installed);

    launching = false;

    disconnect(windowMonitor.data(), SIGNAL(fullscreenWindowOnTopOfOwnWindow()), this, SLOT(stopLaunchProgress()));
}

void LauncherButton::retranslateUi()
{
    if (!model()->desktopEntry().isNull()) {
        setText(model()->desktopEntry()->name());
    }
    MButton::retranslateUi();
}

void LauncherButton::updateFromDesktopEntry(const QString &desktopEntryPath)
{
    if (model()->desktopEntry().isNull() || desktopEntryPath != model()->desktopEntry()->fileName()) {
        QSharedPointer<MDesktopEntry> entry(new MDesktopEntry(desktopEntryPath));
        setText(entry->name());
        action = LauncherAction(desktopEntryPath);
        model()->setDesktopEntry(entry);
    }
}

void LauncherButton::setState(LauncherButtonModel::State state, int progress, const QString &desktopEntryPath)
{
    model()->setButtonState(state);
    if (progress >= 0 && progress <= 100) {
        model()->setOperationProgress(progress);
    }

    // Override the current desktop entry when changing to broken or installed state
    if (state == LauncherButtonModel::Broken || state == LauncherButtonModel::Installed) {
        updateFromDesktopEntry(desktopEntryPath);
    }
}

int LauncherButton::operationProgress() const
{
    return model()->operationProgress();
}
