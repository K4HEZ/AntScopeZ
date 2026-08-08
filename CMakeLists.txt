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
    LinguistTools
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

    # Equivalent of qmake's RC_ICONS: Qt generates the .rc for us. A checked-in
    # AntScope2_resource.rc used to sit here unused -- it hardcoded an absolute
    # path (C:\Projects\AntScope2\AntScope2.ico) and qmake never referenced it
    # either -- and has been deleted rather than renamed for AntScopeZ.
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

    # qt_standard_project_setup() (above) points CMAKE_INSTALL_RPATH at
    # $ORIGIN/../${CMAKE_INSTALL_LIBDIR} -- the *default* Qt lib location.
    # The Install rules section below bundles Qt into a private
    # ${CMAKE_INSTALL_LIBDIR}/antscopez subdirectory instead (to avoid
    # colliding with any distro Qt6 packages actually installed at the
    # default location), so this target needs its own longer INSTALL_RPATH
    # to match. Setting it explicitly here, rather than leaving it to be
    # patched in via file(RPATH_SET) at install time, matters because CMake
    # only reserves as much space for the RPATH string as the *build-time*
    # link step asked for -- a later file(RPATH_SET) to a longer string
    # than that fails outright ("replacement path is too long").
    set_target_properties(AntScopeZ PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/antscopez"
    )

endif()


#
# Translations
#
# .ts sources live in locales/; .qm files are compiled from them at build
# time (lrelease, via qt_add_translations()) instead of being checked into
# the repo as prebuilt binaries. QM_FILES_OUTPUT_VARIABLE keeps them as
# loose files rather than qt_add_translations()'s default of embedding them
# into AntScopeZ's own resources -- loose is what Settings::languageDataFolder()/
# localDataFolder() (see mainwindow.cpp's loadLanguage()) actually read, and
# it's what lets a language be added or overridden by dropping a .qm next to
# the app without a rebuild.
#
# Was: prebuilt QtLanguage_*.qm checked in at the project root, disconnected
# from locales/*.ts -- lrelease's real output went to locales/, which
# nothing ever read, so editing a .ts and regenerating silently never
# reached the running app. release_translations (below) closes that: it's
# built by default, and AntScopeZ is given an explicit dependency on it so
# a parallel build can't stage translations before they exist.
#
# locales/QtLanguage.ts (no language= attribute, 360/360 messages still
# type="unfinished") was a stray lupdate template, not a real locale --
# deleted rather than wired in here.
#
qt_add_translations(
    TARGETS AntScopeZ
    SOURCE_TARGETS AntScopeZ
    TS_FILES
        locales/QtLanguage_uk.ts
        locales/QtLanguage_ja.ts
    QM_FILES_OUTPUT_VARIABLE ANTSCOPE_QM_FILES
)
add_dependencies(AntScopeZ release_translations)

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

set(ANTSCOPE_RUNTIME_FILES
    cables.txt
    itu-regions-defaults.txt
)

if(APPLE)
    # Inside the bundle these belong in Contents/MacOS, next to the binary.
    foreach(_file IN LISTS ANTSCOPE_RUNTIME_FILES)
        set_source_files_properties(${_file} PROPERTIES
            MACOSX_PACKAGE_LOCATION MacOS
        )
    endforeach()
    target_sources(AntScopeZ PRIVATE ${ANTSCOPE_RUNTIME_FILES})
    # ANTSCOPE_QM_FILES are build-tree paths generated by qt_add_translations()
    # above (not source-tree files like ANTSCOPE_RUNTIME_FILES), but the same
    # MACOSX_PACKAGE_LOCATION bundling mechanism applies to them.
    set_source_files_properties(${ANTSCOPE_QM_FILES} PROPERTIES
        MACOSX_PACKAGE_LOCATION MacOS
    )
    target_sources(AntScopeZ PRIVATE ${ANTSCOPE_QM_FILES})
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
    # No staging step needed for ANTSCOPE_QM_FILES here, unlike
    # ANTSCOPE_RUNTIME_FILES above: qt_add_translations() already writes
    # them straight into CMAKE_CURRENT_BINARY_DIR, which for this project's
    # flat, single-config-generator layout is already $<TARGET_FILE_DIR:AntScopeZ>
    # -- the same place the binary itself lands. (A multi-config generator,
    # or moving this call into a nested subdirectory, would break that
    # assumption; revisit if either changes.)
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
    install(FILES ${ANTSCOPE_RUNTIME_FILES} ${ANTSCOPE_QM_FILES}
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
elseif(NOT APPLE) # Linux and other Unix
    install(FILES ${ANTSCOPE_RUNTIME_FILES} ${ANTSCOPE_QM_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/antscopez
    )

    install(FILES antscopez.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )

    install(FILES AntScopeZ.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps
        RENAME antscopez.png
    )

    # Bundle Qt privately under ${CMAKE_INSTALL_LIBDIR}/antscopez instead of
    # linking whatever Qt6 the distro ships. See the "Known issues" section
    # of BUILDINFO.md: a .deb linked against system Qt 6.4.2 showed real bugs
    # (analyzer sometimes refuses to connect on a fresh install, main window
    # doesn't fully repaint after being resized larger, stale paint
    # artifacts) that don't reproduce under Qt 6.11.1 -- confirmed to be a
    # Qt-version difference, not a packaging/install-path problem. This is
    # why the `release` preset (CMakePresets.json) pins CMAKE_PREFIX_PATH at
    # the Qt 6.11 install under /opt rather than relying on find_package(Qt6)
    # picking up whatever's on the system -- that binary is what gets bundled
    # and packaged here.
    #
    # qt_generate_deploy_script()/qt_deploy_runtime_dependencies() are Qt's
    # own CMake-integrated equivalent of linuxdeployqt (introduced Qt 6.3):
    # it walks the executable's actual linked libraries via
    # file(GET_RUNTIME_DEPENDENCIES), copies the non-system ones (Qt itself,
    # plus anything else living under the Qt install) into LIB_DIR/
    # PLUGINS_DIR, sets $ORIGIN-relative RPATHs so the binary finds them
    # without needing LD_LIBRARY_PATH, and (via GENERATE_QT_CONF) writes a
    # qt.conf next to the executable so Qt's own plugin/resource lookup uses
    # the bundled copies too instead of the compiled-in /opt/Qt path. Only
    # the app's own translations are loaded via QTranslator
    # (mainwindow.cpp's m_qtLanguageTranslator, from the QtLanguage_*.qm
    # built by qt_add_translations() above) -- Qt's stock qtbase_*.qm are
    # never used, hence NO_TRANSLATIONS.
    #
    # The install(SCRIPT) runs at `cmake --install`/`cpack` time, well after
    # normal `cmake --build` -- this has no effect on the day-to-day
    # Creator-driven debug/release build+run loop.
    #
    # qt6_generate_deploy_script() (the versioned function) is called
    # directly here rather than the versionless qt_generate_deploy_script()
    # macro -- the macro forwards its arguments on to the function via an
    # unquoted ${ARGV}, and CMake macro-argument substitution is textual, so
    # that extra hop re-parses (and silently expands to empty)
    # ${QT_DEPLOY_BIN_DIR} below despite the backslash escaping it to
    # survive the *first* parse of this CONTENT string. Calling the
    # function directly skips that second, unwanted expansion pass.
    #
    # The pair of file(RPATH_SET) calls bracketing qt_deploy_runtime_dependencies()
    # below work around a real gap in that command: by the time this script
    # runs, install(TARGETS) has already set the executable's RPATH to its
    # final INSTALL_RPATH value (set above, next to LIBUSB) -- but
    # qt_deploy_runtime_dependencies() resolves what to bundle by running
    # file(GET_RUNTIME_DEPENDENCIES) against the binary *as currently
    # installed*, and qt_deploy_runtime_dependencies() only ever rewrites
    # *plugin* RPATHs afterwards, never the main executable's. Left alone,
    # that means it looks for the Qt 6.11 libraries via a RPATH that (quite
    # correctly, for the shipped binary) doesn't mention /opt/Qt at all, and
    # rather than reporting them unresolved it silently falls through to the
    # system's default library search path -- on this dev box that finds a
    # same-SONAME system Qt 6.4 (from the antscope2 package's own Qt
    # dependency, still installed) and bundles *that* instead, since Qt keeps
    # SONAMEs (libQt6Core.so.6, ...) compatible across minor versions. So:
    # point the RPATH at the real Qt 6.11 lib dir first purely so the deploy
    # step resolves against the right copy, then set it back to the
    # INSTALL_RPATH value afterwards for the binary as actually shipped. The
    # swap can't grow the RPATH past what INSTALL_RPATH already reserved
    # space for at link time (file(RPATH_SET) errors "replacement path is
    # too long" otherwise) -- true here since the /opt/Qt path is shorter,
    # but worth knowing if either path ever changes.
    # qt6_generate_deploy_script() doesn't exist in every Qt6 that satisfies
    # this project's find_package(Qt6 6.2 ...) floor -- e.g. the system-qt
    # preset's distro Qt 6.4.2 only provides the higher-level
    # qt6_generate_deploy_app_script(), not this one, so configuring against
    # it would otherwise fail outright with "Unknown CMake command" on a
    # preset that was never meant to bundle Qt in the first place (its whole
    # point is linking the distro Qt directly -- see its description in
    # CMakePresets.json). Skip bundling gracefully rather than hard-erroring
    # when it's unavailable.
    if(COMMAND qt6_generate_deploy_script)

    # Qt6_DIR (set by find_package(Qt6 ...)) is .../gcc_64/lib/cmake/Qt6 --
    # two levels up is the lib/ dir actually containing libQt6Core.so.6 etc.
    get_filename_component(_antscope_qt_lib_dir "${Qt6_DIR}/../.." ABSOLUTE)
    qt6_generate_deploy_script(
        TARGET AntScopeZ
        OUTPUT_SCRIPT deploy_script
        CONTENT "
set(exe_path \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:AntScopeZ>\")
file(RPATH_SET FILE \"\${exe_path}\" NEW_RPATH \"${_antscope_qt_lib_dir}\")
qt_deploy_runtime_dependencies(
    EXECUTABLE \"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:AntScopeZ>\"
    LIB_DIR ${CMAKE_INSTALL_LIBDIR}/antscopez
    PLUGINS_DIR ${CMAKE_INSTALL_LIBDIR}/antscopez/plugins
    GENERATE_QT_CONF
    NO_TRANSLATIONS
)
file(RPATH_SET FILE \"\${exe_path}\" NEW_RPATH \"\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/antscopez\")

# CPack's dpkg-shlibdeps integration (see the Packaging section below) is
# only able to recognize the bundled libraries above as satisfying their
# own SONAMEs via -l<private-dir> if it can find an objdump/shlibs record
# for them there -- it can't, since they're plain copies with no such
# metadata, so for each one it silently falls through to whatever *system*
# package happens to provide the same SONAME instead (Qt SONAMEs are
# compatible across minor versions, so e.g. this dev machine's system
# libqt6core6t64 6.4.2 satisfies libQt6Core.so.6 just as well as the 6.11
# bundled here) -- defeating bundling's whole purpose and, worse, pulling
# in a qt6-base-abi (= 6.4.2) exact-version pin that could make the .deb
# uninstallable on a system whose distro Qt is a different version. A
# debian/shlibs.local file is the standard, documented fix: it tells
# dpkg-shlibdeps to attribute a given SONAME to a package directly rather
# than searching for one, without needing real shlibs/symbols metadata.
# Since debian/control (which dpkg-shlibdeps also consults, to recognize
# *this* package's own name and drop self-dependencies) is deliberately
# left blank by CPack, the antscopez entries below can't be recognized as
# self-references and end up in Depends: too -- but a package trivially
# satisfies a dependency on its own exact version, so apt/dpkg accept it;
# it's just a harmless wart, not a functional problem, unlike the
# alternative (bogus dependency on system Qt) it replaces.
file(GLOB _antscope_bundled_sonames \"\${QT_DEPLOY_PREFIX}/${CMAKE_INSTALL_LIBDIR}/antscopez/lib*.so.*\")
file(WRITE \"\$ENV{DESTDIR}/debian/shlibs.local\" \"\")
foreach(_soname_path IN LISTS _antscope_bundled_sonames)
    get_filename_component(_soname_file \"\${_soname_path}\" NAME)
    if(_soname_file MATCHES \"^(lib[^/]+)\\\\.so\\\\.([0-9]+)\$\")
        file(APPEND \"\$ENV{DESTDIR}/debian/shlibs.local\"
            \"\${CMAKE_MATCH_1} \${CMAKE_MATCH_2} antscopez (>= ${PROJECT_VERSION})\n\")
    endif()
endforeach()
")
    install(SCRIPT ${deploy_script})

    # $ORIGIN-relative RPATH on the binary/plugins (set up above) is not
    # enough by itself for Qt's plugin loader to reliably resolve every
    # symbol it needs from the bundled libraries -- confirmed by testing:
    # running the installed binary as-is reproducibly crashes with
    # "qxcb: ... undefined symbol ... version Qt_6_PRIVATE_API" (a
    # QEventDispatcherGlib symbol Qt's own glib event-dispatcher support
    # needs), even though every individual bundled file resolves to the
    # exact same bytes as the original Qt 6.11 install (verified by
    # hashing them) and the RPATH on every one of them correctly points
    # at LIB_DIR. Explicitly exporting LD_LIBRARY_PATH before exec fixes
    # it -- LD_LIBRARY_PATH is consulted for *every* library the process
    # subsequently loads regardless of which object requested it, unlike
    # RPATH/RUNPATH which (being RUNPATH, not the older always-inherited
    # RPATH) only applies to each object's own direct dependencies. This
    # is the same fix linuxdeployqt-based AppImages typically ship as an
    # AppRun wrapper script, for what appears to be the same class of gap.
    # So: install(TARGETS) above puts the real binary at ${CMAKE_INSTALL_BINDIR}/AntScopeZ
    # (also where qt_deploy_runtime_dependencies() above expects to find
    # it -- this must run after that, not before); rename it to
    # AntScopeZ.bin and put a wrapper script in its place.
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/antscopez-wrapper.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/AntScopeZ-wrapper.sh"
        @ONLY
    )
    # file(INSTALL DESTINATION ...) -- unlike file(RENAME) -- already
    # honors $ENV{DESTDIR} on its own; the DESTINATION below deliberately
    # does *not* include it too, to avoid applying it twice.
    install(CODE "
        file(RENAME \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/AntScopeZ\" \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/AntScopeZ.bin\")
        file(INSTALL \"${CMAKE_CURRENT_BINARY_DIR}/AntScopeZ-wrapper.sh\"
            DESTINATION \"${CMAKE_INSTALL_FULL_BINDIR}\"
            RENAME AntScopeZ
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
    ")

    endif()
endif()


#
# Packaging (.deb)
#
# CPACK_DEBIAN_PACKAGE_SHLIBDEPS auto-computes Depends: by running
# dpkg-shlibdeps against every installed ELF binary -- including the bundled
# Qt libraries/plugins staged above under ${CMAKE_INSTALL_LIBDIR}/antscopez.
# Those bundled libs must not turn into Depends: entries on distro Qt
# packages (the whole point of bundling is to not need them installed), and
# dpkg-shlibdeps has no distro package to attribute a private Qt install to
# anyway. This works out automatically: the deploy script above sets
# $ORIGIN-relative RPATHs on the executable and bundled plugins, and CPack's
# dpkg-shlibdeps integration (requires dpkg-shlibdeps >= 1.17, which Ubuntu/
# Debian have shipped since well before this was written) follows those
# RPATHs and passes the resolved private lib dir to `dpkg-shlibdeps -l`,
# which excludes symbols it resolves there from Depends: while still
# correctly computing Depends: for whatever remains dynamically linked from
# the system (libc, libusb, the X11/Wayland/GL stack, ...).
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
