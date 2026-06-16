set(PACKAGE lipstickplugin)
set(SOURCES lipstickplugin.cpp)
set(HEADERS lipstickplugin.h)

add_library(${PACKAGE} SHARED ${SOURCES} ${HEADERS})

include_directories(
    ../src
    ../src/compositor
    ../src/devicestate
    ../src/qmsystem2
    ../src/touchscreen
    ../src/utilities
)

target_link_libraries(${PACKAGE} PUBLIC
    Qt6::DBus
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickPrivate
    Qt6::Sql
    Qt6::Gui
    Qt6::GuiPrivate
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
    PkgConfig::NGF_QT6
    PkgConfig::THERMALMANAGER_DBUS
    PkgConfig::TIMED_QT
    PkgConfig::WAYLAND_SERVER
    PkgConfig::USB_MODED_QT
    KF6::BluezQt
    lipstick-qt6
    -lrt 
)

install(TARGETS ${PACKAGE} LIBRARY DESTINATION "${KDE_INSTALL_QMLDIR}/org/nemomobile/lipstick")

install(FILES qmldir DESTINATION "${QT_INSTALL_QML}/org/nemomobile/lipstick")
