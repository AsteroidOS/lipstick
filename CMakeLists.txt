cmake_minimum_required(VERSION 3.16.0)

project(
    lipstick
    VERSION 2.1
    DESCRIPTION "QML toolkit for homescreen creation"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(ECM 6.0.0 REQUIRED NO_MODULE)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(FindPkgConfig)
include(FeatureSummary)
include(GNUInstallDirs)
include(ECMFindQmlModule)
include(FindQtWaylandScanner)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(QT_NO_PRIVATE_MODULE_WARNING ON)

set(QT_MIN_VERSION "6.10.0")
find_package(PkgConfig REQUIRED)
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED
    Core
    DBus
    Gui
    Qml
    Quick
    Sql
    Test
    Sensors
    WaylandCompositor
    LinguistTools
)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
  find_package(Qt6 COMPONENTS GuiPrivate QuickPrivate REQUIRED)
endif()
find_package(KF6BluezQt REQUIRED)

pkg_check_modules(DBUS dbus-1 REQUIRED IMPORTED_TARGET)
pkg_check_modules(DSME_DBUS dsme_dbus_if REQUIRED IMPORTED_TARGET)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED IMPORTED_TARGET)
pkg_check_modules(GIO_UNIX gio-unix-2.0 REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBSYSTEMD libsystemd REQUIRED IMPORTED_TARGET)
pkg_check_modules(MLITE mlite6 REQUIRED IMPORTED_TARGET)
pkg_check_modules(MCE mce REQUIRED IMPORTED_TARGET)
pkg_check_modules(MCE_QT mce-qt6 REQUIRED IMPORTED_TARGET)
pkg_check_modules(NEMODEVICELOCK nemodevicelock REQUIRED IMPORTED_TARGET)
pkg_check_modules(NEMOCONNECTIVITY nemoconnectivity REQUIRED IMPORTED_TARGET)
pkg_check_modules(NGF_QT6 ngf-qt6 REQUIRED IMPORTED_TARGET)
pkg_check_modules(THERMALMANAGER_DBUS thermalmanager_dbus_if REQUIRED IMPORTED_TARGET)
pkg_check_modules(TIMED_QT timed-qt6 REQUIRED IMPORTED_TARGET)
pkg_check_modules(WAYLAND_SERVER wayland-server REQUIRED IMPORTED_TARGET)
pkg_check_modules(USB_MODED usb_moded REQUIRED IMPORTED_TARGET)
pkg_check_modules(USB_MODED_QT usb-moded-qt6 REQUIRED IMPORTED_TARGET)
pkg_check_modules(PULSE libpulse REQUIRED IMPORTED_TARGET)
pkg_check_modules(CONTENTACTION contentaction6 IMPORTED_TARGET)

if(CONTENTACTION_FOUND)
    add_definitions("-DHAVE_CONTENTACTION=1")
endif()

add_subdirectory(src)
add_subdirectory(plugin)

add_dependencies(lipstickplugin lipstick-qt6)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

