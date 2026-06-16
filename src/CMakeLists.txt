set(PACKAGE lipstick-qt6)

set(PUBLICHEADERS
    homeapplication.h
    homewindow.h
    lipstickdbus.h
    lipstickglobal.h
    lipstickqmlpath.h
    lipsticksettings.h
    localemanager.h
    shutdownscreen.h
    usbmodeselector.h
    components/launcherdbus.h
    components/launcherfoldermodel.h
    components/launcheritem.h
    components/launchermodel.h
    components/launchermonitor.h
    components/launcherwatchermodel.h
    compositor/lipstickcompositor.h
    compositor/lipstickcompositorprocwindow.h
    compositor/lipstickcompositorwindow.h
    compositor/windowmodel.h
    notifications/lipsticknotification.h
    notifications/notificationlistmodel.h
    notifications/notificationmanager.h
    notifications/notificationpreviewpresenter.h
    utilities/closeeventeater.h 
    utilities/qobjectlistmodel.h
) 

set(SOURCES
    bluetoothagent.cpp
    connectionselector.cpp
    homeapplication.cpp
    homewindow.cpp
    lipstickapi.cpp
    lipstickqmlpath.cpp
    lipsticksettings.cpp
    localemanager.cpp
    screenshotservice.cpp
    shutdownscreen.cpp
    usbmodeselector.cpp
    components/launcherdbus.cpp
    components/launcherfoldermodel.cpp
    components/launcheritem.cpp
    components/launchermodel.cpp
    components/launchermonitor.cpp
    components/launcherwatchermodel.cpp
    compositor/asteroidgesturesextension.cpp
    compositor/lipstickcompositor.cpp
    compositor/lipstickcompositorprocwindow.cpp
    compositor/lipstickcompositorwindow.cpp
    compositor/windowmodel.cpp
    compositor/windowpixmapitem.cpp
    devicelock/devicelock.cpp
    notifications/androidprioritystore.cpp
    notifications/batterynotifier.cpp
    notifications/categorydefinitionstore.cpp
    notifications/diskspacenotifier.cpp
    notifications/lipsticknotification.cpp
    notifications/notificationfeedbackplayer.cpp
    notifications/notificationlistmodel.cpp
    notifications/notificationmanager.cpp
    notifications/notificationpreviewpresenter.cpp
    notifications/thermalnotifier.cpp
    qmsystem2/qmactivity.cpp
    qmsystem2/qmdisplaystate.cpp
    qmsystem2/qmipcinterface.cpp
    qmsystem2/qmlocks.cpp
    qmsystem2/qmsystemstate.cpp
    qmsystem2/qmthermal.cpp
    screenlock/screenlock.cpp
    utilities/closeeventeater.cpp
    utilities/qobjectlistmodel.cpp
    volume/pulseaudiocontrol.cpp
    volume/volumecontrol.cpp
)

add_definitions(-DLIPSTICK_BUILD_LIBRARY)
add_definitions(-DMESA_EGL_NO_X11_HEADERS)
add_definitions(-DQT_COMPOSITOR_QUICK)
add_definitions(-DEGL_NO_X11)
add_definitions(-Wfatal-errors)
add_definitions(-DVERSION="${CMAKE_PROJECT_VERSION}")

qt_add_dbus_adaptor(SOURCES
    "${CMAKE_SOURCE_DIR}/src/notifications/notificationmanager.xml"
    "notifications/notificationmanager.h"
    "NotificationManager"
    "notifications/notificationmanageradaptor"
    "NotificationManagerAdaptor"
)
qt_add_dbus_adaptor(SOURCES
    "${CMAKE_SOURCE_DIR}/src/screenlock/screenlock.xml"
    "screenlock/screenlock.h"
    "ScreenLock"
    "screenlock/screenlockadaptor"
    "ScreenLockAdaptor"
)
qt_add_dbus_adaptor(SOURCES
    "${CMAKE_SOURCE_DIR}/src/screenshotservice.xml"
    "screenshotservice.h"
    "ScreenshotService"
    "screenshotserviceadaptor"
    "ScreenshotServiceAdaptor"
)
qt_add_dbus_adaptor(SOURCES
    "${CMAKE_SOURCE_DIR}/src/shutdownscreen.xml"
    "shutdownscreen.h"
    "ShutdownScreen"
    "shutdownscreenadaptor"
    "ShutdownScreenAdaptor"
)
qt_add_dbus_adaptor(SOURCES
    "${CMAKE_SOURCE_DIR}/src/compositor/compositor.xml"
    "lipstickcompositor.h"
    "LipstickCompositor"
    "lipstickcompositoradaptor"
    "LipstickCompositorAdaptor"
)
qt_add_dbus_adaptor(SOURCES
    "${CMAKE_SOURCE_DIR}/src/devicelock/devicelock.xml"
    "devicelock/devicelock.h"
    "DeviceLock"
    "devicelock/devicelockadaptor"
    "DeviceLockAdaptor"
)
ecm_add_qtwayland_server_protocol(SOURCES
    PROTOCOL "${CMAKE_SOURCE_DIR}/wayland-protocols/asteroid-gestures-unstable-v1.xml"
    BASENAME "asteroid-gestures-unstable-v1"
)

add_library(${PACKAGE} SHARED ${SOURCES} qmsystem2/qmdisplaystate.h)
set_target_properties(${PACKAGE}
    PROPERTIES VERSION ${CMAKE_PROJECT_VERSION}
    SOVERSION ${CMAKE_PROJECT_VERSION})

include_directories(utilities
    3rdparty
    components
    compositor
    devicelock
    qmsystem2
    ${CMAKE_CURRENT_BINARY_DIR}/notifications
    ${CMAKE_CURRENT_BINARY_DIR}/screenlock
    ${CMAKE_CURRENT_BINARY_DIR})

if(CONTENTACTION_FOUND)
    target_link_libraries(${PACKAGE} PUBLIC PkgConfig::CONTENTACTION)
endif()

target_link_libraries(${PACKAGE} PUBLIC
    Qt6::DBus
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickPrivate
    Qt6::Sql
    Qt6::Gui
    Qt6::QuickPrivate
    Qt6::Sensors
    Qt6::WaylandCompositor
    PkgConfig::DBUS
    PkgConfig::DSME_DBUS
    PkgConfig::GLIB2
    PkgConfig::GIO_UNIX
    PkgConfig::LIBSYSTEMD
    PkgConfig::MLITE
    PkgConfig::MCE
    PkgConfig::MCE_QT
    PkgConfig::NEMODEVICELOCK
    PkgConfig::NEMOCONNECTIVITY
    PkgConfig::NGF_QT6
    PkgConfig::THERMALMANAGER_DBUS
    PkgConfig::TIMED_QT
    PkgConfig::WAYLAND_SERVER
    PkgConfig::USB_MODED_QT
    PkgConfig::PULSE
    KF6::BluezQt
    -lrt
)

# Translations
file(GLOB TS_FILES ../i18n/*.ts)
qt_add_lupdate(${PACKAGE} ${TS_FILES} no_check_exist)
qt6_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations DEPENDS ${QM_FILES})
add_dependencies(lipstick-qt6 translations)

install(TARGETS ${PACKAGE} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${PUBLICHEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lipstick-qt6)
install(FILES lipstick.conf DESTINATION /etc/dbus-1/system.d)
install(DIRECTORY notificationcategories
    DESTINATION ${CMAKE_INSTALL_DATADIR}/lipstick/notificationcategories
    FILES_MATCHING PATTERN "*.conf")
install(FILES androidnotificationpriorities DESTINATION ${CMAKE_INSTALL_DATADIR}/lipstick)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/translations)

