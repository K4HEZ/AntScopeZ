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
)

qt_add_qml_module(AntScopeZMobile
    URI AntScopeZ.Mobile
    VERSION 1.0
    QML_FILES
        Main.qml
        ConnectionPage.qml
        ScanPage.qml
)

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
)
