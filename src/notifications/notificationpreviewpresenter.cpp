/***************************************************************************
**
** Copyright (C) 2012 Jolla Ltd.
** Contact: Robin Burchell <robin.burchell@jollamobile.com>
**
** This file is part of lipstick.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include "homewindow.h"
#include "lipsticksettings.h"
#include "utilities/closeeventeater.h"
#include "notifications/notificationmanager.h"
#include "notifications/notificationfeedbackplayer.h"
#include <QScreen> // should be included by lipstickcompositor.h
#include "compositor/lipstickcompositor.h"
#include "notificationpreviewpresenter.h"
#include "lipstickqmlpath.h"

#include <qmdisplaystate.h>
#include <qmlocks.h>

#include <mce/dbus-names.h>

#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QGuiApplication>
#include <QQmlContext>

namespace {

const QString MCE_NOTIFICATION_BEGIN(QStringLiteral("notification_begin_req"));
const QString MCE_NOTIFICATION_END(QStringLiteral("notification_end_req"));

const qint32 MCE_DURATION(4000);
const qint32 MCE_EXTEND_DURATION(2000);
const qint32 MCE_LINGER_DURATION(1000);

enum PreviewMode {
    AllNotificationsEnabled = 0,
    ApplicationNotificationsDisabled,
    SystemNotificationsDisabled,
    AllNotificationsDisabled
};

}

NotificationPreviewPresenter::NotificationPreviewPresenter(QObject *parent) :
    QObject(parent),
    window(0),
    currentNotification(0),
    notificationFeedbackPlayer(new NotificationFeedbackPlayer(this)),
    locks(new MeeGo::QmLocks(this)),
    displayState(new MeeGo::QmDisplayState(this))
{
    connect(NotificationManager::instance(), SIGNAL(notificationAdded(uint)), this, SLOT(updateNotification(uint)));
    connect(NotificationManager::instance(), SIGNAL(notificationRemoved(uint)), this, SLOT(removeNotification(uint)));
    connect(this, SIGNAL(notificationPresented(uint)), notificationFeedbackPlayer, SLOT(addNotification(uint)));

    QTimer::singleShot(0, this, SLOT(createWindowIfNecessary()));
}

NotificationPreviewPresenter::~NotificationPreviewPresenter()
{
    delete window;
}

void NotificationPreviewPresenter::showNextNotification()
{
    if (!LipstickCompositor::instance() && !notificationQueue.isEmpty()) {
        QTimer::singleShot(0, this, SLOT(showNextNotification()));
        return;
    }

    if (notificationQueue.isEmpty()) {
        // No more notifications to show: hide the notification window if it's visible
        if (window != 0 && window->isVisible()) {
            window->hide();
        }

        setCurrentNotification(0);
    } else {
        LipstickNotification *notification = notificationQueue.takeFirst();

        const bool displayOn = notification->hints().value(NotificationManager::HINT_DISPLAY_ON).toBool();
        if (!displayOn &&
            locks->getState(MeeGo::QmLocks::TouchAndKeyboard) == MeeGo::QmLocks::Locked &&
            displayState->get() == MeeGo::QmDisplayState::Off) {
            // Screen locked and off: don't show the notification but just remove it from the queue
            emit notificationPresented(notification->replacesId());

            setCurrentNotification(0);

            showNextNotification();
        } else {
            // Show the notification window and the first queued notification in it
            if (!window->isVisible()) {
                window->show();
            }

            emit notificationPresented(notification->replacesId());

            setCurrentNotification(notification);
        }
    }
}

LipstickNotification *NotificationPreviewPresenter::notification() const
{
    return currentNotification;
}

void NotificationPreviewPresenter::updateNotification(uint id)
{
    LipstickNotification *notification = NotificationManager::instance()->notification(id);

    if (notification != 0) {
        if (notificationShouldBeShown(notification)) {
            // Add the notification to the queue if not already there or the current notification
            if (currentNotification != notification && !notificationQueue.contains(notification)) {
                notificationQueue.append(notification);

                // Show the notification if no notification currently being shown
                if (currentNotification == 0) {
                    showNextNotification();
                }
            }
        } else {
            // Remove updated notification only from the queue so that a currently visible notification won't suddenly disappear
            emit notificationPresented(id);

            removeNotification(id, true);

            if (currentNotification != notification) {
                NotificationManager::instance()->MarkNotificationDisplayed(id);
            }
        }
    }
}

void NotificationPreviewPresenter::removeNotification(uint id, bool onlyFromQueue)
{
    // Remove the notification from the queue
    LipstickNotification *notification = NotificationManager::instance()->notification(id);

    if (notification != 0) {
        notificationQueue.removeAll(notification);

        // If the notification is currently being shown hide it - the next notification will be shown after the current one has been hidden
        if (!onlyFromQueue && currentNotification == notification) {
            currentNotification = 0;
            emit notificationChanged();
        }
    }
}

void NotificationPreviewPresenter::createWindowIfNecessary()
{
    if (window != 0) {
        return;
    }

    window = new HomeWindow();
    window->setGeometry(QRect(QPoint(), QGuiApplication::primaryScreen()->size()));
    window->setCategory(QLatin1String("notification"));
    window->setWindowTitle("Notification");
    window->setContextProperty("initialSize", QGuiApplication::primaryScreen()->size());
    window->setContextProperty("LipstickSettings", LipstickSettings::instance());
    window->setContextProperty("notificationPreviewPresenter", this);
    window->setContextProperty("notificationFeedbackPlayer", notificationFeedbackPlayer);
    window->setSource(QmlPath::to("notifications/NotificationPreview.qml"));
    window->installEventFilter(new CloseEventEater(this));
}

bool NotificationPreviewPresenter::notificationShouldBeShown(LipstickNotification *notification)
{
    if (notification->hidden() || notification->restored() || (notification->previewBody().isEmpty() && notification->previewSummary().isEmpty()))
        return false;

    bool screenOrDeviceLocked = locks->getState(MeeGo::QmLocks::TouchAndKeyboard) == MeeGo::QmLocks::Locked || locks->getState(MeeGo::QmLocks::Device) == MeeGo::QmLocks::Locked;
    int notificationIsCritical = notification->urgency() >= 2 || notification->hints().value(NotificationManager::HINT_DISPLAY_ON).toBool();

    uint mode = AllNotificationsEnabled;
    QWaylandSurface *surface = LipstickCompositor::instance()->surfaceForId(LipstickCompositor::instance()->topmostWindowId());
    if (surface != 0) {
        mode = surface->windowProperties().value("NOTIFICATION_PREVIEWS_DISABLED", uint(AllNotificationsEnabled)).toUInt();
    }

    return (!screenOrDeviceLocked || notificationIsCritical) &&
            (mode == AllNotificationsEnabled ||
             (mode == ApplicationNotificationsDisabled && notificationIsCritical) ||
             (mode == SystemNotificationsDisabled && !notificationIsCritical));
}

void NotificationPreviewPresenter::setCurrentNotification(LipstickNotification *notification)
{
    if (currentNotification != notification) {
        if (currentNotification) {
            const bool displayWasOn(currentNotification->hints().value(NotificationManager::HINT_DISPLAY_ON).toBool());
            if (displayWasOn) {
                // Release our screen wake
                QDBusMessage msg = QDBusMessage::createMethodCall(MCE_SERVICE, MCE_REQUEST_PATH, MCE_REQUEST_IF, MCE_NOTIFICATION_END);
                msg.setArguments(QVariantList() << QString::number(currentNotification->replacesId()) << MCE_LINGER_DURATION);
                QDBusConnection::systemBus().asyncCall(msg);
            }

            NotificationManager::instance()->MarkNotificationDisplayed(currentNotification->replacesId());
        }

        currentNotification = notification;
        emit notificationChanged();

        if (notification) {
            // Ask mce to turn the screen on if requested
            const bool displayOn(notification->hints().value(NotificationManager::HINT_DISPLAY_ON).toBool());
            if (displayOn) {
                if (notification->hints().value(NotificationManager::HINT_DISPLAY_ON).toBool()) {
                    QDBusMessage msg = QDBusMessage::createMethodCall(MCE_SERVICE, MCE_REQUEST_PATH, MCE_REQUEST_IF, MCE_NOTIFICATION_BEGIN);
                    msg.setArguments(QVariantList() << QString::number(notification->replacesId()) << MCE_DURATION << MCE_EXTEND_DURATION);
                    QDBusConnection::systemBus().asyncCall(msg);
                }
            }
        }
    }
}
