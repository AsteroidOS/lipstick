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

#include "notificationmanager.h"
#include "notificationmanagerproxy.h"

#include <QPair>

#include <iostream>
#include <iomanip>
#include <getopt.h>

// The operations for this tool
enum ToolOperation {
    Undefined,
    Add,
    Update,
    Remove
};

// The operation to perform
ToolOperation toolOperation = Undefined;

// The notification ID to use
uint id = 0;

// The icon of the notification
QString icon;

// The category of the notification
QString category;

// The urgency of the notification
int urgency = -1;

// The priority of the notification
int priority = -1;

// The count of items represented by the notification
int count = -1;

// Expire timeout for the notifcation
int expireTimeout = -1;

// Timestamp for notification
QString timestamp;

// Actions for the notification
QHash<QString, QString> actions;

// Hints for the notification
QList<QPair<QString, QString> > hints;

// AppName for the notification
QString appName;

// Prints usage information
int usage(const char *program)
{
    std::cerr << "Usage: " << program << " [OPTION]... [SUMMARY [BODY [PREVIEWSUMMARY [PREVIEWBODY]]]]" << std::endl;
    std::cerr << "Manage notifications." << std::endl;
    std::cerr << std::endl;
    std::cerr << "Mandatory arguments to long options are mandatory for short options too." << std::endl;
    std::cerr << "  -o, --operation=OPERATION  The operation (add/update/remove) to perform." << std::endl;
    std::cerr << "                             add - Adds a new notification." << std::endl;
    std::cerr << "                             update - Updates an existing notification." << std::endl;
    std::cerr << "                             remove - Removes an existing notification." << std::endl;
    std::cerr << "  -i, --id=ID                The notification ID to use when updating or removing a notification." << std::endl;
    std::cerr << "  -u, --urgency=NUMBER       The urgency to assign to the notification." << std::endl;
    std::cerr << "  -p, --priority=NUMBER      The priority to assign to the notification." << std::endl;
    std::cerr << "  -I, --icon=ICON            Icon for the notification."<< std::endl;
    std::cerr << "  -c, --category=CATEGORY    The category of the notification." << std::endl;
    std::cerr << "  -C, --count=NUMBER         The number of items represented by the notification." << std::endl;
    std::cerr << "  -t, --timestamp=TIMESTAMP  Timestamp to use on a notification. Use ISO 8601 extended date format."<< std::endl;
    std::cerr << "  -T, --timeout=MILLISECONDS Expire timeout for the notification in milliseconds or -1 to use server defaults."<< std::endl;
    std::cerr << "  -a, --action=ACTION        An action for the notification in \"ACTIONNAME DBUSSERVICE DBUSPATH DBUSINTERFACE METHOD [ARGUMENTS]...\" format."<< std::endl;
    std::cerr << "  -h, --hint=HINT            A hint to add to the notification, in \"NAME VALUE\" format."<< std::endl;
    std::cerr << "  -A, --application=NAME     The name to use as identifying the application that owns the notification." << std::endl;
    std::cerr << "      --help                 display this help and exit" << std::endl;
    std::cerr << std::endl;
    std::cerr << "A notification ID is mandatory when the operation is 'update' or 'remove'." << std::endl;
    std::cerr << "All options other than -o and -i are ignored when the operation is 'remove'." << std::endl;
    return -1;
}

// Parses command line arguments (the flags)
int parseArguments(int argc, char *argv[])
{
    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            { "operation", required_argument, NULL, 'o' },
            { "id", required_argument, NULL, 'i' },
            { "urgency", required_argument, NULL, 'u' },
            { "priority", required_argument, NULL, 'p' },
            { "icon", required_argument, NULL, 'I' },
            { "category", required_argument, NULL, 'c' },
            { "count", required_argument, NULL, 'C' },
            { "timestamp", required_argument, NULL, 't'},
            { "timeout", required_argument, NULL, 'T' },
            { "action", required_argument, NULL, 'a' },
            { "hint", required_argument, NULL, 'h' },
            { "application", required_argument, NULL, 'A' },
            { "help", no_argument, NULL, 'H' },
            { 0, 0, 0, 0 }
        };

        int c = getopt_long(argc, argv, "o:i:u:p:I:c:C:t:T:a:h:A:", long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'o':
            if (strcmp(optarg, "add") == 0) {
                toolOperation = Add;
            } else if (strcmp(optarg, "update") == 0) {
                toolOperation = Update;
            } else if (strcmp(optarg, "remove") == 0) {
                toolOperation = Remove;
            }
            break;
        case 'i':
            id = atoi(optarg);
            break;
        case 'u':
            urgency = atoi(optarg);
            break;
        case 'p':
            priority = atoi(optarg);
            break;
        case 'I':
            icon = QString(optarg);
            break;
        case 'c':
            category = QString(optarg);
            break;
        case 'C':
            count = atoi(optarg);
            break;
        case 't':
            timestamp = QString(optarg);
            break;
        case 'T':
            expireTimeout = atoi(optarg);
            break;
        case 'h': {
            QStringList hintList = QString(optarg).split(' ');
            if (hintList.count() != 2) {
                toolOperation = Undefined;
            } else {
                hints.append(qMakePair(hintList.at(0), hintList.at(1)));
            }
            break;
            }
        case 'a': {
            QStringList actionList = QString(optarg).split(' ');
            if (actionList.count() < 5) {
                toolOperation = Undefined;
            } else {
                QString name = actionList.takeFirst();
                QString action;
                action.append(actionList.takeFirst()).append(' ');
                action.append(actionList.takeFirst()).append(' ');
                action.append(actionList.takeFirst()).append(' ');
                action.append(actionList.takeFirst());

                foreach(const QString &arg, actionList) {
                    // Serialize the QVariant into a QBuffer
                    QBuffer buffer;
                    buffer.open(QIODevice::ReadWrite);
                    QDataStream stream(&buffer);
                    stream << QVariant(arg);
                    buffer.close();

                    // Encode the contents of the QBuffer in Base64
                    action.append(' ');
                    action.append(buffer.buffer().toBase64().data());
                }

                actions.insert(name, action);
            }
            break;
            }
        case 'A':
            appName = QString(optarg);
            break;
        case 'H':
            return usage(argv[0]);
            break;
        default:
            break;
        }
    }

    if (toolOperation == Undefined ||
            (toolOperation == Add && argc < optind) ||
            (toolOperation == Add && id != 0) ||
            (toolOperation == Update && argc < optind) ||
            (toolOperation == Update && id == 0)) {
        return usage(argv[0]);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    // Parse arguments
    int result = parseArguments(argc, argv);
    if (result != 0) {
        return result;
    }

    QCoreApplication application(argc, argv);
    qDBusRegisterMetaType<QVariantHash>();
    NotificationManagerProxy proxy("org.freedesktop.Notifications", "/org/freedesktop/Notifications", QDBusConnection::sessionBus());

    // Execute the desired operation
    switch (toolOperation) {
    case Add:
    case Update: {
        // Get the parameters for adding and updating notifications
        QString summary, body, previewSummary, previewBody;
        if (argc >= optind) {
            summary = QString(argv[optind]);
        }
        if (argc >= optind + 1) {
            body = QString(argv[optind + 1]);
        }
        if (argc >= optind + 2) {
            previewSummary = QString(argv[optind + 2]);
        }
        if (argc >= optind + 3) {
            previewBody = QString(argv[optind + 3]);
        }

        // Add/update a notification
        QVariantHash hintValues;
        QStringList actionValues;
        if (!category.isEmpty()) {
            hintValues.insert(NotificationManager::HINT_CATEGORY, category);
        }
        if (urgency != -1) {
            hintValues.insert(NotificationManager::HINT_URGENCY, urgency);
        }
        if (priority != -1) {
            hintValues.insert(NotificationManager::HINT_PRIORITY, priority);
        }
        if (count >= 0) {
            hintValues.insert(NotificationManager::HINT_ITEM_COUNT, count);
        }
        if (!timestamp.isEmpty()) {
            hintValues.insert(NotificationManager::HINT_TIMESTAMP, timestamp);
        }
        if (!actions.isEmpty()) {
            foreach (const QString &name, actions.keys()) {
                hintValues.insert(QString(NotificationManager::HINT_REMOTE_ACTION_PREFIX) + name, actions.value(name));
                actionValues << name << QString();
            }
        }
        if (!hints.isEmpty()) {
            QList<QPair<QString, QString> >::const_iterator it = hints.constBegin(), end = hints.constEnd();
            for ( ; it != end; ++it) {
                hintValues.insert(it->first, it->second);
            }
        }
        if (!previewSummary.isEmpty()) {
            hintValues.insert(NotificationManager::HINT_PREVIEW_SUMMARY, previewSummary);
        }
        if (!previewBody.isEmpty()) {
            hintValues.insert(NotificationManager::HINT_PREVIEW_BODY, previewBody);
        }
        if (appName.isEmpty()) {
            appName = argv[0];
        }
        result = proxy.Notify(appName, id, icon, summary, body, actionValues, hintValues, expireTimeout);
        break;
    }
    case Remove:
        if (id > 0) {
            proxy.CloseNotification(id);
        }
        break;
    default:
        break;
    }

    return result;
}
