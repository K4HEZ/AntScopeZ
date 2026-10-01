#
# AntScopeZ mobile -- Qt Quick front end over antscopez_core.
# Its own top-level project: open this file (not the root one) with an
# Android kit, or a desktop kit to try the screens on the PC.
#

cmake_minimum_required(VERSION 3.21)

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/../version.txt ANTSCOPEZ_VERSION_TXT LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../version.txt)

project(AntScopeZMobile
    VERSION ${ANTSCOPEZ_VERSION_TXT}
    LANGUAGES C CXX
)
set(ANTSCOPEZ_VER_SUFFIX "-dev")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Bluetooth)
qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(../core ${CMAKE_BINARY_DIR}/core)

qt_add_executable(AntScopeZMobile
    main.cpp
    analyzercontroller.cpp
    analyzercontroller.h
    swrchart.cpp
    swrchart.h
    smithchart.cpp
    smithchart.h
    bandpresets.cpp
    bandpresets.h
    mobilesettings.h
)

qt_add_qml_module(AntScopeZMobile
    URI AntScopeZ.Mobile
    VERSION 1.0
    QML_FILES
        Main.qml
        ConnectionPage.qml
        ScanPage.qml
        SwrPage.qml
        SmithPage.qml
        LiveDataPage.qml
        ImpedancePage.qml
        SettingsPage.qml
        AboutPage.qml
        ParametersGrid.qml
)

# The desktop ships shared/itu-regions-defaults.txt next to its binary via
# AppPaths::sharedDataFolder() (Linux/macOS install layout); mobile has no
# such install step (and Android has no filesystem access to it at all), so
# bundle the same file as a Qt resource instead -- one source file, no copy.
qt_add_resources(AntScopeZMobile "itu_bands"
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/../shared
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/../shared/itu-regions-defaults.txt
)

qt_add_resources(AntScopeZMobile "icon"
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/../desktop/resources
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/../desktop/resources/AntScopeZ.png
)

# Same per-build timestamp header the desktop About box uses (see the root
# CMakeLists.txt), for About's "Build" line.
add_custom_target(generate_build_timestamp
    COMMAND ${CMAKE_COMMAND}
        "-DIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/../cmake/build-timestamp.h.in"
        "-DOUT_FILE=${CMAKE_BINARY_DIR}/build-timestamp.h"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/generate-build-timestamp.cmake"
    BYPRODUCTS "${CMAKE_BINARY_DIR}/build-timestamp.h"
)
add_dependencies(AntScopeZMobile generate_build_timestamp)
target_include_directories(AntScopeZMobile PRIVATE ${CMAKE_BINARY_DIR})

target_compile_definitions(AntScopeZMobile PRIVATE
    ANTSCOPEZ_VER="${PROJECT_VERSION}${ANTSCOPEZ_VER_SUFFIX}"
)

target_link_libraries(AntScopeZMobile PRIVATE
    antscopez_core
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(AntScopeZMobile PROPERTIES
    QT_ANDROID_PACKAGE_NAME "io.github.k4hez.antscopez"
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    # Custom AndroidManifest.xml (USB host support, USB permission receiver)
    # and UsbPermissionReceiver.java -- see mobile/android/'s own files.
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
)
