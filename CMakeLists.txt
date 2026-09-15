cmake_minimum_required(VERSION 3.16)

project(cutefish-settings
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(QT Core Gui Widgets Quick QuickControls2 DBus Xml Concurrent LinguistTools)
find_package(Qt6 REQUIRED ${QT})

include(GNUInstallDirs)
find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Libcrypt REQUIRED)
# find_package(QApt REQUIRED)

find_package(KF6NetworkManagerQt REQUIRED)
find_package(KF6ModemManagerQt REQUIRED)
# KF6Config dropped: KF6::ConfigCore was linked but never used in src/
# (terminal pattern — dead KF6 links removed; unversioned find_package
# breaks generation when kf6-kconfig-devel is absent).

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cutefish-framework/applications
                 ${CMAKE_CURRENT_BINARY_DIR}/cutefish-framework-applications-build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cutefish-framework/appearance
                 ${CMAKE_CURRENT_BINARY_DIR}/cutefish-framework-appearance-build)

pkg_search_module(FontConfig REQUIRED fontconfig IMPORTED_TARGET)
pkg_check_modules(ICU REQUIRED IMPORTED_TARGET icu-i18n icu-uc)

include_directories(${ICU_INCLUDE_DIRS})

set(SRCS
    src/application.cpp
    src/main.cpp
    src/docksettings.cpp
    src/battery.cpp
    src/brightness.cpp
    src/fontsmodel.cpp
    src/language.cpp
    src/about.cpp
    src/notifications.cpp
    src/password.cpp
    src/batteryhistorymodel.cpp
    src/cicontheme.cpp
    src/fonts/fonts.cpp
    src/powermanager.cpp
    src/cursor/cursorthememodel.cpp
    src/cursor/cursortheme.cpp
    src/cursor/mouse.cpp
    src/datetime/time.cpp
    src/datetime/timezonedata.h
    src/datetime/timezonemap.cpp
    src/datetime/timedated_interface.cpp
    src/touchpad.cpp
    src/vpn/vpn.cpp
    src/vpn/nm-l2tp-service.h
    src/vpn/nm-openvpn-service.h
    src/vpn/nm-pptp-service.h
    src/networkproxy.cpp
    src/defaultapplications.cpp
#    src/update/updatemodel.cpp
)

set(RESOURCES
    src/resources.qrc
)

qt6_add_dbus_adaptor(DBUS_SOURCES
                     src/com.cutefish.SettingsUI.xml
                     src/application.h Application)
set_source_files_properties(${DBUS_SOURCES} PROPERTIES SKIP_AUTOGEN ON)

add_executable(${PROJECT_NAME} ${DBUS_SOURCES} ${SRCS} ${RESOURCES})
# Private-prefix install: the binary links libcutefish-framework-appearance.so
# from <prefix>/lib64 (installed by the framework's own selective build).
# RPATH lets it start without LD_LIBRARY_PATH (fishui pattern dd96285).
set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib64"
)
target_link_libraries(${PROJECT_NAME}
        Cutefish::Applications
        Cutefish::Appearance
        Qt6::Core
        Qt6::Widgets
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::DBus
        Qt6::Xml
        Qt6::Concurrent
        # FishUI

        KF6::NetworkManagerQt
        KF6::ModemManagerQt

        PkgConfig::FontConfig
        ${FREETYPE_LIBRARIES}
        PkgConfig::ICU
        Libcrypt::Libcrypt

        # QApt
)

file(GLOB TS_FILES translations/*.ts)
qt6_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations DEPENDS ${QM_FILES} SOURCES ${TS_FILES})
add_dependencies(${PROJECT_NAME} translations)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
    cutefish-settings.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications/
    COMPONENT Runtime
)

install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/translations)
