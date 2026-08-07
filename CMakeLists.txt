cmake_minimum_required(VERSION 3.21)

project(AntScopeZ
    VERSION 2.1.4
    DESCRIPTION "AntScopeZ Antenna Analyzer Application"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to a Release build for single-config generators, matching the way
# the qmake build was normally driven.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# CMake's own default install prefix is /usr/local; for a .deb (or any FHS
# install) it needs to be /usr instead. Doing this via CMAKE_INSTALL_PREFIX
# itself (rather than only CPack's separate CPACK_PACKAGING_INSTALL_PREFIX)
# matters because ANTSCOPE_SHARED_DATA_DIR below is computed from
# CMAKE_INSTALL_FULL_DATADIR at *configure* time and baked into the binary
# -- a plain `cmake --install .` and CPack's .deb need to agree on it, and
# CPACK_PACKAGING_INSTALL_PREFIX only affects the latter.
# CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT is unset (not just false) if
# CMake itself hasn't already forced a default -- only override in that
# case, so an explicit `-DCMAKE_INSTALL_PREFIX=...` is still respected.
if(UNIX AND NOT APPLE AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

include(GNUInstallDirs)


#
# Qt
#

find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    PrintSupport
    SerialPort
    Network
    Xml
    Concurrent
    OpenGL
    Bluetooth
)

# Enables AUTOMOC / AUTOUIC.
qt_standard_project_setup()

# qt_standard_project_setup() only defaults CMAKE_AUTOMOC/CMAKE_AUTOUIC to ON
# if unset -- it does NOT do the same for CMAKE_AUTORCC (confirmed by reading
# Qt6CoreMacros.cmake for this Qt version). Without this, res.qrc is listed as
# a source of the AntScopeZ target below but is never actually run through
# rcc, so every resource in it (checkbox indicator icons, rig_logo.png,
# logo_watermark.png, pencil.png, the app icon) silently fails to load at
# runtime -- QDir(":/new/prefix1").entryList() comes back empty. Must be set
# before qt_add_executable() so the target picks it up at creation time.
set(CMAKE_AUTORCC ON)


#
# Project options
#
# ANTSCOPE_NEW_CONNECTION, ANTSCOPE_NEW_ANALYZER, and ANTSCOPE_OLD_TDR used to
# gate old code paths they replaced; all three flags were permanently ON, and
# the OFF-path code they guarded no longer compiles (it refers to
# types/arrays that don't exist any more, or mismatched Analyzer/AnalyzerPro
# types), so the flags and the dead code are gone. ANTSCOPE_DEBUG_BLE's raw
# TX/RX qDebug() calls are commented out at their call sites in
# ble_analyzer.cpp instead of a compile-time option -- uncomment locally when
# actually debugging Bluetooth.
#

#
# Sources
#

set(ANTSCOPE_SOURCES
    AA55BTPacket.cpp
    antscopeupdatedialog.cpp
    appregistrationdialog.cpp
    calibration.cpp
    centralwidget.cpp
    crc32.cpp
    customgraph.cpp
    CustomPlot.cpp
    editbandsdialog.cpp
    encodinghelpers.cpp
    export.cpp
    filedialog.cpp
    fqsettings.cpp
    glwidget.cpp
    inforequestdialog.cpp
    licenseagent.cpp
    main.cpp
    mainwindow.cpp
    markers.cpp
    markerspopup.cpp
    measurements.cpp
    modelesspopup.cpp
    Notification.cpp
    onefqwidget.cpp
    popup.cpp
    popupindicator.cpp
    presets.cpp
    print.cpp
    printmarkers.cpp
    printmulti.cpp
    ProgressDlg.cpp
    qcustomplot.cpp
    screenshot.cpp
    selectdevicedialog.cpp
    settings.cpp
    style.cpp
    unitrequestdialog.cpp
    updatedialog.cpp

    analyzer/analyzerdata.cpp
    analyzer/analyzerpro.cpp
    analyzer/baseanalyzer.cpp
    analyzer/ble_analyzer.cpp
    analyzer/com_analyzer.cpp
    analyzer/customanalyzer.cpp
    analyzer/hid_analyzer.cpp
    analyzer/nanovna_analyzer.cpp
    analyzer/updater/aa230firmwareupdater.cpp
    analyzer/updater/aa30zerofirmwareupdater.cpp
    analyzer/updater/downloader.cpp
    analyzer/updater/firmwareupdater.cpp
    analyzer/updater/hidfirmwareupdater.cpp

    devinfo/redeviceinfo.cpp
    ftdi/ftdiinfo.cpp
)

set(ANTSCOPE_HEADERS
    AA55BTPacket.h
    antscopeupdatedialog.h
    appregistrationdialog.h
    calibration.h
    centralwidget.h
    crc32.h
    customgraph.h
    CustomPlot.h
    editbandsdialog.h
    encodinghelpers.h
    export.h
    filedialog.h
    fqinputvalidator.h
    fqsettings.h
    glwidget.h
    inforequestdialog.h
    licenseagent.h
    mainwindow.h
    markers.h
    markerspopup.h
    measurements.h
    modelesspopup.h
    Notification.h
    onefqwidget.h
    popup.h
    popupindicator.h
    presets.h
    print.h
    printmarkers.h
    printmulti.h
    ProgressDlg.h
    qcustomplot.h
    screenshot.h
    selectdevicedialog.h
    settings.h
    style.h
    unitrequestdialog.h
    updatedialog.h

    analyzer/analyzerdata.h
    analyzer/analyzerparameters.h
    analyzer/analyzerpro.h
    analyzer/baseanalyzer.h
    analyzer/ble_analyzer.h
    analyzer/com_analyzer.h
    analyzer/customanalyzer.h
    analyzer/hid_analyzer.h
    analyzer/nanovna_analyzer.h
    analyzer/usbhid/hidapi/hidapi.h
    analyzer/updater/aa230firmwareupdater.h
    analyzer/updater/aa30zerofirmwareupdater.h
    analyzer/updater/downloader.h
    analyzer/updater/firmwareupdater.h
    analyzer/updater/hidfirmwareupdater.h

    devinfo/redeviceinfo.h
    ftdi/ftd2xx.h
    ftdi/ftdiinfo.h
)

set(ANTSCOPE_FORMS
    antscopeupdatedialog.ui
    appregistrationdialog.ui
    editbandsdialog.ui
    export.ui
    fqsettings.ui
    inforequestdialog.ui
    mainwindow.ui
    modelesspopup.ui
    print.ui
    ProgressDlg.ui
    screenshot.ui
    selectdevicedialog.ui
    settings.ui
    unitrequestdialog.ui
    updatedialog.ui

    analyzer/analyzerdata.ui
)


#
# Executable
#

qt_add_executable(AntScopeZ
    ${ANTSCOPE_SOURCES}
    ${ANTSCOPE_HEADERS}
    ${ANTSCOPE_FORMS}
    res.qrc
)


#
# Compile definitions
#

target_compile_definitions(AntScopeZ PRIVATE
    ANTSCOPEZ_VER="${PROJECT_VERSION}"
)

# The qmake build defined _DEBUG in debug configurations.
target_compile_definitions(AntScopeZ PRIVATE $<$<CONFIG:Debug>:_DEBUG>)


#
# Include directories
#
# The project root is required because several headers are included as
# <analyzer/usbhid/hidapi/hidapi.h> and <devinfo/redeviceinfo.h>.
#

target_include_directories(AntScopeZ PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/analyzer
    ${CMAKE_CURRENT_SOURCE_DIR}/analyzer/updater
)


#
# Qt libraries
#

target_link_libraries(AntScopeZ PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::SerialPort
    Qt6::Network
    Qt6::Xml
    Qt6::Concurrent
    Qt6::OpenGL
    Qt6::Bluetooth
)


#
# Platform handling
#

if(WIN32)

    target_sources(AntScopeZ PRIVATE
        analyzer/usbhid/hidapi/windows/hid.c
    )

    target_link_libraries(AntScopeZ PRIVATE setupapi)

    target_include_directories(AntScopeZ PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/ftdi
    )

    # Equivalent of qmake's RC_ICONS: Qt generates the .rc for us. The checked-in
    # AntScopeZ_resource.rc is not used -- it hardcodes an absolute path
    # (C:\Projects\AntScopeZ\AntScopeZ.ico) and qmake never referenced it either.
    set_target_properties(AntScopeZ PROPERTIES
        WIN32_EXECUTABLE      ON
        QT_TARGET_RC_ICONS    "${CMAKE_CURRENT_SOURCE_DIR}/AntScopeZ.ico"
    )

    # Ship the FTDI driver DLL next to the executable.
    add_custom_command(TARGET AntScopeZ POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/ftdi/amd64/ftd2xx.dll"
            "$<TARGET_FILE_DIR:AntScopeZ>"
        COMMENT "Copying ftd2xx.dll next to AntScopeZ"
        VERBATIM
    )

    # NOTE: AntScope.pro had a "win64" scope carrying the only OpenSSL link
    # flags in the project. qmake has no such scope, so those flags were never
    # actually applied. They are deliberately not reproduced here -- add them
    # only once it is confirmed Windows needs them.

elseif(APPLE)

    target_sources(AntScopeZ PRIVATE
        analyzer/usbhid/hidapi/mac/hid.c
    )

    target_compile_definitions(AntScopeZ PRIVATE _NO_WINDOWS_ NO_MULTITAB)

    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(AntScopeZ PRIVATE ${COREFOUNDATION_FRAMEWORK})

    set_source_files_properties(AntScopeZ.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(AntScopeZ PRIVATE AntScopeZ.icns)

    set_target_properties(AntScopeZ PROPERTIES
        MACOSX_BUNDLE                ON
        MACOSX_BUNDLE_INFO_PLIST     "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
        MACOSX_BUNDLE_ICON_FILE      AntScopeZ
        MACOSX_BUNDLE_BUNDLE_NAME    AntScopeZ
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )

else() # Linux and other Unix

    target_sources(AntScopeZ PRIVATE
        analyzer/usbhid/hidapi/linux/hid.c
    )

    target_compile_definitions(AntScopeZ PRIVATE _NO_WINDOWS_)

    # Settings::sharedDataFolder() -- see the Install rules section below and
    # the comment on that function for why this needs to match
    # CMAKE_INSTALL_PREFIX rather than being hardcoded to /usr.
    target_compile_definitions(AntScopeZ PRIVATE
        ANTSCOPE_SHARED_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/antscopez"
    )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
    target_link_libraries(AntScopeZ PRIVATE PkgConfig::LIBUSB)

endif()


#
# Runtime data files
#
# The application reads these from disk next to the executable, so they are
# copied into the build output. This mirrors the RUNTIME_FILES / TRANSLATION_FILES
# COPIES rules in AntScope.pro.
#
# NOTE: itu-regions.txt is intentionally absent -- the qmake build never copied
# it either. Whether the non-defaults file is needed at runtime is still open.
#
# NOTE: the .qm files are taken from the project root, which is where the qmake
# build took them from. The .ts sources live in locales/, so `lrelease` writes
# its output somewhere the build does not read. That mismatch predates this
# CMake port and is deliberately not changed here.
#

set(ANTSCOPE_RUNTIME_FILES
    cables.txt
    itu-regions-defaults.txt
    QtLanguage_ja.qm
    QtLanguage_uk.qm
)

if(APPLE)
    # Inside the bundle these belong in Contents/MacOS, next to the binary.
    foreach(_file IN LISTS ANTSCOPE_RUNTIME_FILES)
        set_source_files_properties(${_file} PROPERTIES
            MACOSX_PACKAGE_LOCATION MacOS
        )
    endforeach()
    target_sources(AntScopeZ PRIVATE ${ANTSCOPE_RUNTIME_FILES})
else()
    foreach(_file IN LISTS ANTSCOPE_RUNTIME_FILES)
        add_custom_command(TARGET AntScopeZ POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/${_file}"
                "$<TARGET_FILE_DIR:AntScopeZ>"
            COMMENT "Staging ${_file}"
            VERBATIM
        )
    endforeach()
endif()


#
# Install rules
#
# macOS/Windows keep the original "everything next to the binary" layout
# (Settings::programDataPath()/languageDataFolder() still look there on
# those platforms). Linux gets a real FHS layout: binary in bindir, shared
# read-only data in datadir/antscopez (see ANTSCOPE_SHARED_DATA_DIR above
# and Settings::sharedDataFolder()), plus a .desktop file and icon so it
# shows up in application menus.
#

install(TARGETS AntScopeZ
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
    install(FILES ${ANTSCOPE_RUNTIME_FILES}
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
elseif(NOT APPLE) # Linux and other Unix
    install(FILES ${ANTSCOPE_RUNTIME_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/antscopez
    )

    install(FILES antscopez.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )

    install(FILES AntScopeZ.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps
        RENAME antscopez.png
    )
endif()


#
# Packaging (.deb)
#
# CPACK_DEBIAN_PACKAGE_SHLIBDEPS auto-computes Depends: by running
# dpkg-shlibdeps against the built binary's actual linked libraries -- which
# only maps to real package names if AntScopeZ was configured against
# distro-packaged Qt6 (plain find_package(Qt6 ...), no CMAKE_PREFIX_PATH
# pointing at a separately-installed Qt SDK; dpkg has no package to
# attribute a private Qt install to).
#

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "antscopez")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    # TODO: fill in a real maintainer contact before actually distributing
    # this .deb to anyone else -- left as just the GitHub handle for now
    # rather than guessing at what contact info should be public.
    set(CPACK_PACKAGE_CONTACT "hz23116")
    set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/hz23116/AntScopeZ")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    # Matches the CMAKE_INSTALL_PREFIX default set near the top of this file
    # -- kept explicit here so it can't silently drift from the
    # already-compiled-in ANTSCOPE_SHARED_DATA_DIR if CPack's own DEB
    # default (also /usr) ever changes.
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

    include(CPack)
endif()
