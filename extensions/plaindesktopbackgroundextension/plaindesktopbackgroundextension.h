/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of duihome.
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

#ifndef PLAINDESKTOPBACKGROUNDEXTENSION_H_
#define PLAINDESKTOPBACKGROUNDEXTENSION_H_

#include <QObject>
#include <QTimeLine>
#include <MGConfItem>
#include "mdesktopbackgroundextensioninterface.h"

class QPixmap;
class WindowInfo;
class PlainDesktopBackgroundPixmap;

/*!
 * An extension that draws the desktop background without any special effects.
 */
class PlainDesktopBackgroundExtension : public QObject, public MDesktopBackgroundExtensionInterface
{
    Q_OBJECT
    Q_INTERFACES(MDesktopBackgroundExtensionInterface MApplicationExtensionInterface)

public:
    /*!
     * Constructs a PlainDesktopBackgroundExtension.
     */
    PlainDesktopBackgroundExtension();

    /*!
     * Destroys the PlainDesktopBackgroundExtension.
     */
    virtual ~PlainDesktopBackgroundExtension();

    //! \reimp
    virtual void setDesktopInterface(MDesktopInterface &desktopInterface);
    virtual void drawBackground(QPainter *painter, const QRectF &boundingRect) const;
    virtual bool initialize(const QString &interface);
    virtual MWidget *widget();
    //! \reimp_end

private slots:
    /*!
     * \brief Sets the blur time line direction based on whether there are windows in the window list.
     */
    void setBlurTimeLineDirection(const QList<WindowInfo> &windowList);

    /*!
     * Sets the blur factor.
     *
     * \param blurFactor the new blur factor
     */
    void setBlurFactor(qreal blurFactor);

    /*!
     * Updates the landscape pixmap.
     */
    void updateLandscapePixmap();

    /*!
     * Updates the landscape pixmap.
     */
    void updatePortraitPixmap();

private:
    /*!
     * Updates a pixmap.
     *
     * \param pixmap the pixmap to be updated
     * \param gConfItem the MGConfItem to get the pixmap name from
     * \param defaultName the name of the default pixmap to be used if loading the user selected one fails
     */
    void updatePixmap(QSharedPointer<PlainDesktopBackgroundPixmap> *pixmap, MGConfItem &gConfItem, const QString &defaultName);

    //! An interface for getting information about the state of the desktop
    MDesktopInterface *desktop;

    //! GConf item for the landscape background image
    MGConfItem landscapeGConfItem;

    //! GConf item for the portrait background image
    MGConfItem portraitGConfItem;

    //! The landscape desktop background pixmap
    QSharedPointer<PlainDesktopBackgroundPixmap> landscapePixmap;

    //! The portrait desktop background pixmap
    QSharedPointer<PlainDesktopBackgroundPixmap> portraitPixmap;

    //! The name of the landscape default background image
    QString landscapeDefaultBackgroundImage;

    //! The name of the portrait default background image
    QString portraitDefaultBackgroundImage;

    //! Blurring factor
    qreal blurFactor;

    //! Blurring timeline
    QTimeLine blurTimeLine;

    //! Blur radius (in pixels)
    int blurRadius;

    //! Whether a pixmap is being updated or not
    bool pixmapBeingUpdated;
};

#endif /* PLAINDESKTOPBACKGROUNDEXTENSION_H_ */
