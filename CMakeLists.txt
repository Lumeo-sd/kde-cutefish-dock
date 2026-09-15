cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME cutefish-terminal)
project(${PROJECT_NAME})

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT Core Gui Quick QuickControls2 Widgets DBus LinguistTools Core5Compat)
find_package(Qt6 6.5 REQUIRED ${QT})

include(GNUInstallDirs)

# Get the QML installation directory from qtpaths (Qt6), overridable so a
# private-prefix install can target ~/.local/lib64/qt6/qml.
find_program(QT_PATHS_EXECUTABLE qtpaths6)
if(NOT QT_PATHS_EXECUTABLE)
    message(FATAL_ERROR "qtpaths is not found.")
endif()

if(NOT INSTALL_QMLDIR)
    execute_process(COMMAND ${QT_PATHS_EXECUTABLE} -query QT_INSTALL_QML
        OUTPUT_VARIABLE INSTALL_QMLDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()
set(INSTALL_QMLDIR "${INSTALL_QMLDIR}" CACHE PATH "QML import installation directory")
if(INSTALL_QMLDIR)
    message(STATUS "qml directory:" "${INSTALL_QMLDIR}")
else()
    message(FATAL_ERROR "qml directory cannot be detected.")
endif()

set(SRCS
    src/main.cpp
    src/processhelper.h
    src/processhelper.cpp
    src/utils.cpp
    src/fonts.cpp
)

set(RESOURCES
    src/resources.qrc
)

add_subdirectory(qmltermwidget)
add_executable(${PROJECT_NAME} ${SRCS} ${RESOURCES})
target_link_libraries(${PROJECT_NAME}
        Qt6::Core
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
        Qt6::DBus
)

file(GLOB TS_FILES translations/*.ts)
qt6_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations DEPENDS ${QM_FILES} SOURCES ${TS_FILES})
add_dependencies(${PROJECT_NAME} translations)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/translations)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
    cutefish-terminal.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications/
    COMPONENT Runtime
)
