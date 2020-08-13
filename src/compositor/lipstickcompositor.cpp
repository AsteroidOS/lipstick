/***************************************************************************
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: Aaron Kennedy <aaron.kennedy@jollamobile.com>
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

#ifdef HAVE_CONTENTACTION
#include <contentaction.h>
#endif

#include <QWaylandSeat>
#include <QDesktopServices>
#include <QtSensors/QOrientationSensor>
#include <QClipboard>
#include <QSettings>
#include <QMimeData>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include "homeapplication.h"
#include "windowmodel.h"
#include "lipstickcompositorprocwindow.h"
#include "lipstickcompositor.h"
#include "lipstickcompositoradaptor.h"
#include "lipsticksettings.h"
#include <qpa/qwindowsysteminterface.h>
#include "hwcrenderstage.h"
#include <private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformintegration.h>
#include <QWaylandQuickShellSurfaceItem>
#include <QtWaylandCompositor/private/qwlextendedsurface_p.h>

#include <mce/dbus-names.h>
#include <mce/mode-names.h>


#define MCE_DISPLAY_LPM_SET_SUPPORTED "set_lpm_supported"


LipstickCompositor *LipstickCompositor::m_instance = 0;

LipstickCompositor::LipstickCompositor()
    : m_totalWindowCount(0)
    , m_nextWindowId(1)
    , m_homeActive(true)
    , m_shaderEffect(0)
    , m_fullscreenSurface(0)
    , m_directRenderingActive(false)
    , m_topmostWindowId(0)
    , m_topmostWindowProcessId(0)
    , m_topmostWindowOrientation(Qt::PrimaryOrientation)
    , m_screenOrientation(Qt::PrimaryOrientation)
    , m_sensorOrientation(Qt::PrimaryOrientation)
    , m_displayState(0)
    , m_retainedSelection(0)
    , m_currentDisplayState(MeeGo::QmDisplayState::Unknown)
    , m_updatesEnabled(true)
    , m_completed(false)
    , m_onUpdatesDisabledUnfocusedWindowId(0)
    , m_fakeRepaintTriggered(false)
{
    m_window = new QQuickWindow();
    m_window->setColor(Qt::black);
    m_window->setVisible(true);

    m_output = new QWaylandQuickOutput(this, m_window);
    m_output->setSizeFollowsWindow(true);
    connect(this, &QWaylandCompositor::surfaceCreated, this, &LipstickCompositor::onSurfaceCreated);

    m_wlShell = new QWaylandWlShell(this);
    connect(m_wlShell, &QWaylandWlShell::wlShellSurfaceCreated, this, &LipstickCompositor::onShellSurfaceCreated);

    m_surfExtGlob = new QtWayland::SurfaceExtensionGlobal(this);
    connect(m_surfExtGlob, &QtWayland::SurfaceExtensionGlobal::extendedSurfaceReady, this, &LipstickCompositor::onExtendedSurfaceReady);

    m_wm = new QWaylandQtWindowManager(this);
    connect(m_wm, &QWaylandQtWindowManager::openUrl, this, &LipstickCompositor::openUrl);

    setRetainedSelectionEnabled(true);

    if (m_instance) qFatal("LipstickCompositor: Only one compositor instance per process is supported");
    m_instance = this;

    m_orientationLock = new MGConfItem("/lipstick/orientationLock", this);
    connect(m_orientationLock, SIGNAL(valueChanged()), SIGNAL(orientationLockChanged()));

    // Load legacy settings from the config file and delete it from there
    QSettings legacySettings("nemomobile", "lipstick");
    QString legacyOrientationKey("Compositor/orientationLock");
    if (legacySettings.contains(legacyOrientationKey)) {
        m_orientationLock->set(legacySettings.value(legacyOrientationKey));
        legacySettings.remove(legacyOrientationKey);
    }

    connect(m_window, SIGNAL(visibleChanged(bool)), this, SLOT(onVisibleChanged(bool)));
    QObject::connect(HomeApplication::instance(), SIGNAL(aboutToDestroy()), this, SLOT(homeApplicationAboutToDestroy()));

    m_orientationSensor = new QOrientationSensor(this);
    /*QObject::connect(m_orientationSensor, SIGNAL(readingChanged()), this, SLOT(setScreenOrientationFromSensor()));
    if (!m_orientationSensor->connectToBackend()) {
        qWarning() << "Could not connect to the orientation sensor backend";
    } else {
        if (!m_orientationSensor->start())
            qWarning() << "Could not start the orientation sensor";
    }*/
    emit HomeApplication::instance()->homeActiveChanged();

    QDesktopServices::setUrlHandler("http", this, "openUrl");
    QDesktopServices::setUrlHandler("https", this, "openUrl");
    QDesktopServices::setUrlHandler("mailto", this, "openUrl");

    connect(QGuiApplication::clipboard(), SIGNAL(dataChanged()), SLOT(clipboardDataChanged()));

    HwcRenderStage::initialize(this);

    m_timedDbus = new Maemo::Timed::Interface();
    if( !m_timedDbus->isValid() )
    {
      qWarning() << "invalid dbus interface:" << m_timedDbus->lastError();
    }

    QTimer::singleShot(0, this, SLOT(initialize()));
}

LipstickCompositor::~LipstickCompositor()
{
    // ~QWindow can a call into onVisibleChanged and QWaylandCompositor after we
    // are destroyed, so disconnect it.
    disconnect(m_window, SIGNAL(visibleChanged(bool)), this, SLOT(onVisibleChanged(bool)));

    delete m_timedDbus;
    delete m_shaderEffect;
}

LipstickCompositor *LipstickCompositor::instance()
{
    return m_instance;
}

void LipstickCompositor::homeApplicationAboutToDestroy()
{
    m_window->hide();
    m_window->releaseResources();

    // When destroying LipstickCompositor ~QQuickWindow() is called after
    // ~QWaylandQuickCompositor(), so changes to the items in the window may end
    // up calling code such as LipstickCompositorWindow::handleTouchCancel(),
    // which will try to use the compositor, at that point not usable anymore.
    // So delete all the windows here.
    foreach (LipstickCompositorWindow *w, m_windows) {
        delete w;
    }

    m_instance = 0;
    delete this;
}

void LipstickCompositor::onVisibleChanged(bool visible)
{
    if (!visible) {
        m_output->sendFrameCallbacks();
    }
}

static LipstickCompositorWindow *surfaceWindow(QWaylandSurface *surface)
{
    return surface->views().isEmpty() ? 0 : static_cast<LipstickCompositorWindow *>(surface->views().first()->renderObject());
}

void LipstickCompositor::onShellSurfaceCreated(QWaylandWlShellSurface *shellSurface)
{
    QWaylandSurface *surface = shellSurface->surface();
    LipstickCompositorWindow *window = surfaceWindow(surface);
    if(window) {
//        window->setWlShellSurface(shellSurface);
        connect(shellSurface, &QWaylandWlShellSurface::titleChanged, this, &LipstickCompositor::surfaceTitleChanged);
        connect(shellSurface, &QWaylandWlShellSurface::setTransient, this, &LipstickCompositor::surfaceSetTransient);
        connect(shellSurface, &QWaylandWlShellSurface::setFullScreen, this, &LipstickCompositor::surfaceSetFullScreen);
    }
}

void LipstickCompositor::onSurfaceCreated(QWaylandSurface *surface)
{
    LipstickCompositorWindow *item = surfaceWindow(surface);
    if (!item)
        item = createView(surface);
    connect(surface, SIGNAL(hasContentChanged()), this, SLOT(onHasContentChanged()));
    connect(surface, SIGNAL(sizeChanged()), this, SLOT(surfaceSizeChanged()));
    connect(surface, SIGNAL(damaged(QRegion)), this, SLOT(surfaceDamaged(QRegion)));
    connect(surface, SIGNAL(redraw()), this, SLOT(windowSwapped()));
}

void LipstickCompositor::onExtendedSurfaceReady(QtWayland::ExtendedSurface *extSurface, QWaylandSurface *surface)
{
    LipstickCompositorWindow *window = surfaceWindow(surface);
    if(window)
        window->setExtendedSurface(extSurface);
}

bool LipstickCompositor::openUrl(QWaylandClient *client, const QUrl &url)
{
    Q_UNUSED(client)
#if defined(HAVE_CONTENTACTION)
    ContentAction::Action action = url.scheme() == "file"? ContentAction::Action::defaultActionForFile(url.toString()) : ContentAction::Action::defaultActionForScheme(url.toString());
    if (action.isValid()) {
        action.trigger();
    }
    return action.isValid();
#else
    Q_UNUSED(url)
    return false;
#endif
}

void LipstickCompositor::retainedSelectionReceived(QMimeData *mimeData)
{
    if (!m_retainedSelection)
        m_retainedSelection = new QMimeData;

    // Make a copy to allow QClipboard to take ownership of our data
    m_retainedSelection->clear();
    foreach (const QString &format, mimeData->formats())
        m_retainedSelection->setData(format, mimeData->data(format));

    QGuiApplication::clipboard()->setMimeData(m_retainedSelection.data());
}

int LipstickCompositor::windowCount() const
{
    return m_mappedSurfaces.count();
}

int LipstickCompositor::ghostWindowCount() const
{
    return m_totalWindowCount - windowCount();
}

bool LipstickCompositor::homeActive() const
{
    return m_homeActive;
}

void LipstickCompositor::setHomeActive(bool a)
{
    if (a == m_homeActive)
        return;

    m_homeActive = a;

    emit homeActiveChanged();
    emit HomeApplication::instance()->homeActiveChanged();
}

bool LipstickCompositor::debug() const
{
    static enum { Yes, No, Unknown } status = Unknown;
    if (status == Unknown) {
        QByteArray v = qgetenv("LIPSTICK_COMPOSITOR_DEBUG");
        bool value = !v.isEmpty() && v != "0" && v != "false";
        if (value) status = Yes;
        else status = No;
    }
    return status == Yes;
}

QObject *LipstickCompositor::windowForId(int id) const
{
    QObject *window = m_windows.value(id, NULL);
    return window;
}

void LipstickCompositor::closeClientForWindowId(int id)
{
    LipstickCompositorWindow *window = m_windows.value(id, 0);
    if (window && window->surface())
        destroyClientForSurface(window->surface());
}

QWaylandSurface *LipstickCompositor::surfaceForId(int id) const
{
    LipstickCompositorWindow *window = m_windows.value(id, 0);
    return window?window->surface():0;
}

bool LipstickCompositor::completed()
{
    return m_completed;
}

int LipstickCompositor::windowIdForLink(QWaylandSurface *s, uint link) const
{
    for (QHash<int, LipstickCompositorWindow *>::ConstIterator iter = m_windows.begin();
        iter != m_windows.end(); ++iter) {

        QWaylandSurface *windowSurface = iter.value()->surface();
        LipstickCompositorWindow *window = surfaceWindow(windowSurface);

        if (windowSurface && windowSurface->client() && s->client() && window &&
            windowSurface->client()->processId() == s->client()->processId() &&
            window->windowProperties().value("WINID", uint(0)).toUInt() == link)
            return iter.value()->windowId();
    }

    return 0;
}

void LipstickCompositor::clearKeyboardFocus()
{
//    defaultInputDevice()->setKeyboardFocus(NULL);
}

void LipstickCompositor::setDisplayOff()
{
    if (!m_displayState) {
        qWarning() << "No display";
        return;
    }

    m_displayState->set(MeeGo::QmDisplayState::Off);
}

void LipstickCompositor::surfaceDamaged(const QRegion &)
{
    if (!m_window->isVisible()) {
        // If the compositor is not visible, do not throttle.
        // make it conditional to QT_WAYLAND_COMPOSITOR_NO_THROTTLE?
        m_output->sendFrameCallbacks();
    }
}

void LipstickCompositor::setFullscreenSurface(QWaylandSurface *surface)
{
    if (surface == m_fullscreenSurface)
        return;

    // Prevent flicker when returning to composited mode
    if (!surface && m_fullscreenSurface) {
        foreach (QWaylandView *view, m_fullscreenSurface->views())
            static_cast<LipstickCompositorWindow *>(view->renderObject())->update();
    }

    m_fullscreenSurface = surface;

    emit fullscreenSurfaceChanged();
}

QObject *LipstickCompositor::clipboard() const
{
    return QGuiApplication::clipboard();
}

void LipstickCompositor::setTopmostWindowId(int id)
{
    if (id != m_topmostWindowId) {
        m_topmostWindowId = id;
        emit topmostWindowIdChanged();

        int pid = -1;
        QWaylandSurface *surface = surfaceForId(m_topmostWindowId);

        if (surface)
            pid = surface->client()->processId();

        if (m_topmostWindowProcessId != pid) {
            m_topmostWindowProcessId = pid;
            emit privateTopmostWindowProcessIdChanged(m_topmostWindowProcessId);
        }
    }
}

LipstickCompositorWindow *LipstickCompositor::createView(QWaylandSurface *surface)
{
    int id = m_nextWindowId++;
    LipstickCompositorWindow *item = new LipstickCompositorWindow(id, "", surface, m_window->contentItem());
    QObject::connect(item, SIGNAL(destroyed(QObject*)), this, SLOT(windowDestroyed()));
    m_windows.insert(item->windowId(), item);
    return item;
}

void LipstickCompositor::onSurfaceDying()
{
    QWaylandSurface *surface = static_cast<QWaylandSurface *>(sender());
    LipstickCompositorWindow *item = surfaceWindow(surface);

    if (surface == m_fullscreenSurface)
        setFullscreenSurface(0);

    if (item) {
        item->m_windowClosed = true;
        item->tryRemove();
    }
}

void LipstickCompositor::initialize()
{
    m_displayState = new MeeGo::QmDisplayState(this);
    MeeGo::QmDisplayState::DisplayState displayState = m_displayState->get();
    reactOnDisplayStateChanges(displayState);
    connect(m_displayState, SIGNAL(displayStateChanged(MeeGo::QmDisplayState::DisplayState)), this, SLOT(reactOnDisplayStateChanges(MeeGo::QmDisplayState::DisplayState)));

    new LipstickCompositorAdaptor(this);

    QDBusConnection systemBus = QDBusConnection::systemBus();
    if (!systemBus.registerService("org.nemomobile.compositor")) {
        qWarning("Unable to register D-Bus service org.nemomobile.compositor: %s", systemBus.lastError().message().toUtf8().constData());
    }
    if (!systemBus.registerObject("/", this)) {
        qWarning("Unable to register object at path /: %s", systemBus.lastError().message().toUtf8().constData());
    }
    QDBusMessage message = QDBusMessage::createMethodCall(MCE_SERVICE, MCE_REQUEST_PATH, MCE_REQUEST_IF, MCE_DISPLAY_LPM_SET_SUPPORTED);
    message.setArguments(QVariantList() << ambientSupported());
    QDBusConnection::systemBus().asyncCall(message);
}

void LipstickCompositor::windowDestroyed(LipstickCompositorWindow *item)
{
    int id = item->windowId();

    m_windows.remove(id);
    surfaceUnmapped(item);
}

void LipstickCompositor::onHasContentChanged()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());

    if(surface->isCursorSurface())
        return;

    if(surface->hasContent())
        surfaceMapped(surface);
    else
        surfaceUnmapped(surface);
}

void LipstickCompositor::surfaceMapped(QWaylandSurface *surface)
{
    LipstickCompositorWindow *item = surfaceWindow(surface);
    if (!item)
        item = createView(surface);

    // The surface was mapped for the first time
    if (item->m_mapped)
        return;

    QVariantMap properties = item->windowProperties();

    item->m_mapped = true;
    item->m_category = properties.value("CATEGORY").toString();

    if (!item->parentItem()) {
        // TODO why contentItem?
        item->setParentItem(m_window->contentItem());
    }

    item->setSize(surface->size());
    QObject::connect(surface, &QWaylandSurface::surfaceDestroyed, this, &LipstickCompositor::onSurfaceDying);

    m_totalWindowCount++;
    m_mappedSurfaces.insert(item->windowId(), item);

    item->setTouchEventsEnabled(true);

    emit windowCountChanged();
    emit windowAdded(item);

    windowAdded(item->windowId());

    emit availableWinIdsChanged();
}

void LipstickCompositor::surfaceSizeChanged()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());

    LipstickCompositorWindow *window = surfaceWindow(surface);
    if (window)
        window->setSize(surface->size());
}

void LipstickCompositor::surfaceTitleChanged()
{
    QWaylandWlShellSurface *wlShellSurface = qobject_cast<QWaylandWlShellSurface*>(sender());
    LipstickCompositorWindow *window = surfaceWindow(wlShellSurface->surface());
    if (window) {
        window->setTitle(wlShellSurface->title());
        emit window->titleChanged();

        int windowId = window->windowId();

        for (int ii = 0; ii < m_windowModels.count(); ++ii)
            m_windowModels.at(ii)->titleChanged(windowId);
    }
}

void LipstickCompositor::surfaceSetTransient(QWaylandSurface *transientParent, const QPoint &relativeToParent, bool inactive)
{
    Q_UNUSED(inactive)
    QWaylandWlShellSurface *wlShellSurface = qobject_cast<QWaylandWlShellSurface*>(sender());
    LipstickCompositorWindow *window = surfaceWindow(wlShellSurface->surface());
    if (window && transientParent) {
        LipstickCompositorWindow *transientParentItem = surfaceWindow(transientParent);
        if (transientParentItem) {
            window->setParentItem(transientParentItem);
            window->setX(relativeToParent.x());
            window->setY(relativeToParent.y());
        } else {
            qWarning("Surface was mapped without visible transient parent");
        }
    }
}

void LipstickCompositor::surfaceSetFullScreen(QWaylandWlShellSurface::FullScreenMethod method, uint framerate, QWaylandOutput *output)
{
    Q_UNUSED(method)
    Q_UNUSED(framerate)
    QWaylandWlShellSurface *wlShellSurface = qobject_cast<QWaylandWlShellSurface*>(sender());

    QWaylandOutput *designatedOutput = output ? output : m_output;
    if (!designatedOutput)
        return;

    wlShellSurface->sendConfigure(designatedOutput->geometry().size(), QWaylandWlShellSurface::NoneEdge);
}

void LipstickCompositor::windowSwapped()
{
    m_output->sendFrameCallbacks();
}

void LipstickCompositor::windowDestroyed()
{
    m_totalWindowCount--;
    m_windows.remove(static_cast<LipstickCompositorWindow *>(sender())->windowId());
    emit ghostWindowCountChanged();
}

void LipstickCompositor::surfaceUnmapped(QWaylandSurface *surface)
{
    if (surface == m_fullscreenSurface)
        setFullscreenSurface(0);

    LipstickCompositorWindow *window = surfaceWindow(surface);
    if (window)
        emit windowHidden(window);
}

void LipstickCompositor::surfaceUnmapped(LipstickCompositorWindow *item)
{
    int id = item->windowId();

    if (m_mappedSurfaces.remove(id) == 0) {
        // It was unmapped already so nothing to do
        return;
    }

    emit windowCountChanged();
    emit windowRemoved(item);

    item->m_windowClosed = true;
    item->tryRemove();

    emit ghostWindowCountChanged();

    windowRemoved(id);

    emit availableWinIdsChanged();
}

void LipstickCompositor::windowAdded(int id)
{
    for (int ii = 0; ii < m_windowModels.count(); ++ii)
        m_windowModels.at(ii)->addItem(id);
}

void LipstickCompositor::windowRemoved(int id)
{
    for (int ii = 0; ii < m_windowModels.count(); ++ii)
        m_windowModels.at(ii)->remItem(id);
}

QQmlComponent *LipstickCompositor::shaderEffectComponent()
{
    const char *qml_source =
        "import QtQuick 2.0\n"
        "ShaderEffect {\n"
            "property QtObject window\n"
            "property ShaderEffectSource source: ShaderEffectSource { sourceItem: window }\n"
        "}";

    if (!m_shaderEffect) {
        m_shaderEffect = new QQmlComponent(qmlEngine(this));
        m_shaderEffect->setData(qml_source, QUrl());
    }
    return m_shaderEffect;
}

void LipstickCompositor::setTopmostWindowOrientation(Qt::ScreenOrientation topmostWindowOrientation)
{
    if (m_topmostWindowOrientation != topmostWindowOrientation) {
        m_topmostWindowOrientation = topmostWindowOrientation;
        emit topmostWindowOrientationChanged();
    }
}

void LipstickCompositor::setScreenOrientation(Qt::ScreenOrientation screenOrientation)
{
    if (m_screenOrientation != screenOrientation) {
        if (debug())
            qDebug() << "Setting screen orientation on QWaylandCompositor";

        QSize physSize = m_output->physicalSize();
        switch(screenOrientation) {
        case Qt::PrimaryOrientation:
            m_output->setTransform(QWaylandOutput::TransformNormal);
            break;
        case Qt::LandscapeOrientation:
            if(physSize.width() > physSize.height())
                m_output->setTransform(QWaylandOutput::TransformNormal);
            else
                m_output->setTransform(QWaylandOutput::Transform90);
            break;
        case Qt::PortraitOrientation:
            if(physSize.width() > physSize.height())
                m_output->setTransform(QWaylandOutput::Transform90);
            else
                m_output->setTransform(QWaylandOutput::TransformNormal);
            break;
        case Qt::InvertedLandscapeOrientation:
            if(physSize.width() > physSize.height())
                m_output->setTransform(QWaylandOutput::Transform180);
            else
                m_output->setTransform(QWaylandOutput::Transform270);
            break;
        case Qt::InvertedPortraitOrientation:
            if(physSize.width() > physSize.height())
                m_output->setTransform(QWaylandOutput::Transform270);
            else
                m_output->setTransform(QWaylandOutput::Transform180);
            break;
        }
        QWindowSystemInterface::handleScreenOrientationChange(qApp->primaryScreen(),screenOrientation);

        m_screenOrientation = screenOrientation;
        emit screenOrientationChanged();
    }
}

void LipstickCompositor::reactOnDisplayStateChanges(MeeGo::QmDisplayState::DisplayState state)
{
    if (m_currentDisplayState == state) {
        return;
    }

    if (state == MeeGo::QmDisplayState::On) {
        emit displayOn();
    } else if (state == MeeGo::QmDisplayState::Off) {
        QCoreApplication::postEvent(this, new QTouchEvent(QEvent::TouchCancel));
        emit displayOff();
    }

    bool changeInDimming = (state == MeeGo::QmDisplayState::Dimmed) != (m_currentDisplayState == MeeGo::QmDisplayState::Dimmed);

    bool changeInAmbient = ((state == MeeGo::QmDisplayState::Off) != (m_currentDisplayState == MeeGo::QmDisplayState::Off)) && ambientEnabled();

    bool enterAmbient = changeInAmbient && (state == MeeGo::QmDisplayState::Off);
    bool leaveAmbient = changeInAmbient && (state != MeeGo::QmDisplayState::Off);

    m_currentDisplayState = state;

    if (changeInDimming) {
        emit displayDimmedChanged();
    }
    if (changeInAmbient) {
        emit displayAmbientChanged();
    }
    if (enterAmbient) {
        emit displayAmbientEntered();
    }
    if (leaveAmbient) {
        emit displayAmbientLeft();
    }
}

void LipstickCompositor::setScreenOrientationFromSensor()
{
    QOrientationReading* reading = m_orientationSensor->reading();

    if (debug())
        qDebug() << "Screen orientation changed " << reading->orientation();

    Qt::ScreenOrientation sensorOrientation = m_sensorOrientation;
    switch (reading->orientation()) {
        case QOrientationReading::TopUp:
            sensorOrientation = Qt::PortraitOrientation;
            break;
        case QOrientationReading::TopDown:
            sensorOrientation = Qt::InvertedPortraitOrientation;
            break;
        case QOrientationReading::LeftUp:
            sensorOrientation = Qt::InvertedLandscapeOrientation;
            break;
        case QOrientationReading::RightUp:
            sensorOrientation = Qt::LandscapeOrientation;
            break;
        case QOrientationReading::FaceUp:
        case QOrientationReading::FaceDown:
            /* Keep screen orientation at previous state */
            break;
        case QOrientationReading::Undefined:
        default:
            sensorOrientation = Qt::PrimaryOrientation;
            break;
    }

    if (sensorOrientation != m_sensorOrientation) {
        m_sensorOrientation = sensorOrientation;
        emit sensorOrientationChanged();
    }
}

void LipstickCompositor::clipboardDataChanged()
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    if (mimeData && mimeData != m_retainedSelection)
        overrideSelection(const_cast<QMimeData *>(mimeData));
}

bool LipstickCompositor::ambientSupported() const
{
    void* ambientMode = QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("AmbientSupported");
    if (ambientMode) {
        return true;
    }
    return false;
}

void LipstickCompositor::setAmbientEnabled(bool enabled)
{
    if (!ambientSupported()) {
        return;
    }

    if (m_ambientModeEnabled == enabled) {
        return;
    }

    m_ambientModeEnabled = enabled;
    if (m_ambientModeEnabled) {
        QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("AmbientEnable");
    } else {
        QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("AmbientDisable");
    }
    emit ambientEnabledChanged();
}

void LipstickCompositor::scheduleAmbientUpdate()
{
    if (!ambientEnabled()) {
        return;
    }
    QMap<QString,QVariant> match;
    match.insert("type", QVariant(QString("wakeup")));
    QDBusReply< QList<QVariant> > reply = m_timedDbus->query_sync(match);

    if( !reply.isValid() ) {
        qWarning() << "'query' call failed:" << m_timedDbus->lastError();
        return;
    }

    uint cookie = 0;
    QList<QVariant> cookies = reply.value();
    // Cancel all wakeup cookies except one.
    while (!cookies.isEmpty()) {
        bool ok = true;
        cookie = cookies.takeFirst().toUInt(&ok);
        if (!ok) {
            cookie = 0;
            continue;
        }
        // If the current cookie isn't the last one in the list.
        if (!cookies.isEmpty()) {
            QDBusReply<bool> res = m_timedDbus->cancel_sync(cookie);
            if (!res.isValid()) {
                qWarning() << "'cancel' call failed:" << m_timedDbus->lastError();
            } else {
                qWarning() << "cookie " << cookie << " deleted " << res.value();
            }
        }
    }

    // Add new wakeup event
    Maemo::Timed::Event wakeupEvent;

    time_t currentTime;
    struct tm* timeinfo;
    time(&currentTime);
    // We don't want to update the screen 60 seconds after the screen is off.
    // The screen should be updated when the minute digit changes.
    timeinfo = localtime(&currentTime);
    timeinfo->tm_sec = 0;

    time_t wakeupTime = mktime(timeinfo);
    if (wakeupTime == -1) {
        wakeupTime = currentTime;
    }
    wakeupTime += 60;
    wakeupEvent.setTicker(wakeupTime);
    wakeupEvent.setAttribute(QLatin1String("APPLICATION"), QLatin1String("wakup_alarm"));
    wakeupEvent.setAttribute(QLatin1String("type"), QLatin1String("wakeup"));
    wakeupEvent.setBootFlag();
    wakeupEvent.setKeepAliveFlag();
    wakeupEvent.setReminderFlag();
    wakeupEvent.setAlarmFlag();
    wakeupEvent.setSingleShotFlag();

    if (cookie) {
        QDBusReply<uint> res = m_timedDbus->replace_event_sync(wakeupEvent, cookie);
    } else {
        QDBusReply<uint> res = m_timedDbus->add_event_sync(wakeupEvent);
    }
}

void LipstickCompositor::setAmbientUpdatesEnabled(bool enabled)
{
    if (enabled) {
        if (m_currentDisplayState == MeeGo::QmDisplayState::On) {
            return;
        }
        if (!ambientEnabled()) {
            return;
        }
    }
    setUpdatesEnabled(enabled, true);;
    if (enabled) {
        emit displayAmbientUpdate();
    }
}

void LipstickCompositor::setUpdatesEnabled(bool enabled, bool inAmbientMode)
{
    if ((m_updatesEnabled != enabled) || inAmbientMode) {
        if (!inAmbientMode) {
            m_updatesEnabled = enabled;
        }
        if (!enabled) {
            emit displayAboutToBeOff();
            LipstickCompositorWindow *topmostWindow = qobject_cast<LipstickCompositorWindow *>(windowForId(topmostWindowId()));
            if (topmostWindow != 0 && topmostWindow->hasFocus()) {
                m_onUpdatesDisabledUnfocusedWindowId = topmostWindow->windowId();
                clearKeyboardFocus();
            }
            m_window->hide();
            if (m_window->handle() && !inAmbientMode) {
                QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("DisplayOff");
            }
            // trigger frame callbacks which are pending already at this time
            surfaceCommitted();

            scheduleAmbientUpdate();
        } else {
            if (m_window->handle() && !inAmbientMode) {
                QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("DisplayOn");
            }
            emit displayAboutToBeOn();
            m_window->showFullScreen();
            if (m_onUpdatesDisabledUnfocusedWindowId > 0) {
                if (!LipstickSettings::instance()->lockscreenVisible()) {
                    LipstickCompositorWindow *topmostWindow = qobject_cast<LipstickCompositorWindow *>(windowForId(topmostWindowId()));
                    if (topmostWindow != 0 && topmostWindow->windowId() == m_onUpdatesDisabledUnfocusedWindowId) {
                        topmostWindow->takeFocus();
                    }
                }
                m_onUpdatesDisabledUnfocusedWindowId = 0;
            }
        }
    }

    if (enabled && !m_completed) {
        m_completed = true;
        emit completedChanged();
    }
}

void LipstickCompositor::surfaceCommitted()
{
    if (!m_window->isVisible() && !m_fakeRepaintTriggered) {
        startTimer(1000);
        m_fakeRepaintTriggered = true;
    }
}

void LipstickCompositor::timerEvent(QTimerEvent *e)
{
    Q_UNUSED(e)

    m_output->frameStarted();
    m_output->sendFrameCallbacks();
    killTimer(e->timerId());
}
