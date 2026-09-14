cmake_minimum_required(VERSION 3.14)

project(statusbar LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(Qt6 CONFIG REQUIRED Widgets DBus Concurrent LinguistTools QuickControls2)
find_package(KF6WindowSystem REQUIRED)

# wlr-layer-shell: the bar is a top panel with a reserved screen edge.
find_package(LayerShellQt REQUIRED)
# org_kde_plasma_window_management + org_kde_kwin_appmenu, for window
# tracking and the application menu on Wayland.
find_package(KWayland REQUIRED)

set(SRCS
    src/main.cpp
    src/statusbar.cpp
    src/controlcenterdialog.cpp
    src/brightness.cpp
    src/battery.cpp
    src/appearance.cpp
    src/processprovider.cpp
    src/activity.cpp
    src/capplications.cpp
    src/windowmanager.cpp
    src/notifications.cpp
    src/backgroundhelper.cpp
    src/keyboardlayout.cpp
    src/bluetoothcontrol.cpp

    src/systemtray/statusnotifieritemsource.cpp
    src/systemtray/systemtraytypes.cpp
    src/systemtray/systemtraytypedefs.h
    src/systemtray/systemtraymodel.cpp
    src/systemtray/statusnotifierwatcher.cpp
    src/systemtray/statusnotifieritemhost.cpp

    src/libdbusmenuqt/dbusmenuimporter.cpp
    src/libdbusmenuqt/dbusmenushortcut_p.cpp
    src/libdbusmenuqt/dbusmenutypes_p.cpp
    src/libdbusmenuqt/utils.cpp

    src/appmenu/kdbusimporter.h
    src/appmenu/appmenu.h
    src/appmenu/appmenu.cpp
    src/appmenu/appmenudbus.h
    src/appmenu/appmenudbus.cpp
    src/appmenu/verticalmenu.h
    src/appmenu/verticalmenu.cpp
    src/appmenu/menuimporter.h
    src/appmenu/menuimporter.cpp
    src/appmenu/appmenumodel.h
    src/appmenu/appmenumodel.cpp
    src/appmenu/appmenuapplet.h
    src/appmenu/appmenuapplet.cpp
    src/appmenu/dbusmenu_interface.cpp
    src/appmenu/dbusmenu_interface.h

    src/poweractions.cpp

    qml.qrc
)

set(statusnotifierwatcher_xml src/systemtray/org.kde.StatusNotifierWatcher.xml)
qt6_add_dbus_interface(SRCS ${statusnotifierwatcher_xml} statusnotifierwatcher_interface)
qt6_add_dbus_interface(SRCS src/systemtray/org.freedesktop.DBus.Properties.xml dbusproperties)

set(statusnotifieritem_xml src/systemtray/org.kde.StatusNotifierItem.xml)
set_source_files_properties(${statusnotifieritem_xml} PROPERTIES
   NO_NAMESPACE false
   INCLUDE "src/systemtray/systemtraytypes.h"
   CLASSNAME OrgKdeStatusNotifierItem
)
qt6_add_dbus_interface(SRCS ${statusnotifieritem_xml} statusnotifieritem_interface)

qt6_add_dbus_adaptor(SRCS src/systemtray/org.kde.StatusNotifierWatcher.xml
                     src/systemtray/statusnotifierwatcher.h StatusNotifierWatcher)

# libdbusmenuqt
set_source_files_properties(src/libdbusmenuqt/com.canonical.dbusmenu.xml PROPERTIES
   NO_NAMESPACE true
   INCLUDE "src/libdbusmenuqt/dbusmenutypes_p.h"
   CLASSNAME DBusMenuInterface
)

# The interface was added on Qt5.15
# qt_add_dbus_interface(libdbusmenu_SRCS src/libdbusmenuqt/com.canonical.dbusmenu.xml dbusmenu_interface)

## appmenu
qt6_add_dbus_adaptor(appmenu_SRCS src/appmenu/com.canonical.AppMenu.Registrar.xml
    src/appmenu/menuimporter.h MenuImporter menuimporteradaptor MenuImporterAdaptor)
qt6_add_dbus_adaptor(appmenu_SRCS src/appmenu/com.cutefish.cappmenu.xml
    src/appmenu/appmenudbus.h AppmenuDBus appmenuadaptor AppmenuAdaptor)

## statusbar dbus
qt6_add_dbus_adaptor(DBUS_SOURCES
                     src/com.cutefish.Statusbar.xml
                     src/statusbar.h StatusBar)

add_executable(cutefish-statusbar ${SRCS} ${appmenu_SRCS} ${DBUS_SOURCES})

target_link_libraries(cutefish-statusbar
  PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Quick
  Qt6::QuickControls2
  Qt6::Concurrent
  Qt6::DBus
  KF6::WindowSystem
  Plasma::KWaylandClient
  LayerShellQt::Interface
)

file(GLOB TS_FILES translations/*.ts)
qt6_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations DEPENDS ${QM_FILES} SOURCES ${TS_FILES})
add_dependencies(cutefish-statusbar translations)

install(TARGETS cutefish-statusbar RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/cutefish-statusbar/translations)
