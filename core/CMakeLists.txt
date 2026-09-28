#
# antscopez_core -- the UI-free library shared by desktop/ and mobile/,
# pulled in with add_subdirectory(). Links no Widgets or Gui, so a widget
# dependency creeping in fails the build.
#

find_package(Qt6 REQUIRED COMPONENTS Core SerialPort Network Xml Concurrent Bluetooth)
include(GNUInstallDirs)

set(ANTSCOPE_CORE_SOURCES
    AA55BTPacket.cpp
    appconfig.cpp
    apppaths.cpp
    calibration.cpp
    crc32.cpp
    debuglog.cpp
    markermath.cpp
    measurementdata.cpp
    measurementfiles.cpp
    measurementsession.cpp
    rfmath.cpp
    tdrmath.cpp

    analyzer/analyzerpro.cpp
    analyzer/baseanalyzer.cpp
    analyzer/ble_analyzer.cpp
    analyzer/com_analyzer.cpp
    analyzer/customanalyzer.cpp
    analyzer/hid_analyzer.cpp
    analyzer/nanovna_analyzer.cpp
    analyzer/nanovna_v2_analyzer.cpp
    analyzer/updater/aa230firmwareupdater.cpp
    analyzer/updater/aa30zerofirmwareupdater.cpp
    analyzer/updater/downloader.cpp
    analyzer/updater/firmwareupdater.cpp
    analyzer/updater/hidfirmwareupdater.cpp

    devinfo/redeviceinfo.cpp
    ftdi/ftdiinfo.cpp

    remoteapi/remoteapiconnection.cpp
    remoteapi/remoteapiserver.cpp
)

set(ANTSCOPE_CORE_HEADERS
    AA55BTPacket.h
    appconfig.h
    apppaths.h
    calibration.h
    crc32.h
    debuglog.h
    markerlist.h
    markermath.h
    measurementdata.h
    measurementfiles.h
    measurementlist.h
    measurementsession.h
    rfmath.h
    tdrmath.h
    usermessage.h

    analyzer/analyzerparameters.h
    analyzer/analyzerpro.h
    analyzer/baseanalyzer.h
    analyzer/ble_analyzer.h
    analyzer/com_analyzer.h
    analyzer/customanalyzer.h
    analyzer/hid_analyzer.h
    analyzer/nanovna_analyzer.h
    analyzer/nanovna_v2_analyzer.h
    analyzer/usbhid/hidapi/hidapi.h
    analyzer/updater/aa230firmwareupdater.h
    analyzer/updater/aa30zerofirmwareupdater.h
    analyzer/updater/downloader.h
    analyzer/updater/firmwareupdater.h
    analyzer/updater/hidfirmwareupdater.h

    devinfo/redeviceinfo.h
    ftdi/ftd2xx.h
    ftdi/ftdiinfo.h

    remoteapi/remoteapiconnection.h
    remoteapi/remoteapihost.h
    remoteapi/remoteapiprotocol.h
    remoteapi/remoteapiserver.h
)

qt_add_library(antscopez_core STATIC
    ${ANTSCOPE_CORE_SOURCES}
    ${ANTSCOPE_CORE_HEADERS}
)

target_compile_definitions(antscopez_core PRIVATE
    ANTSCOPEZ_VER="${PROJECT_VERSION}${ANTSCOPEZ_VER_SUFFIX}"
    CALIBRATION_DEBUG_TOOLS=0
    $<$<CONFIG:Debug>:_DEBUG>
)
if(NOT WIN32)
    target_compile_definitions(antscopez_core PUBLIC _NO_WINDOWS_)
endif()

# core/ is the include root, so <analyzer/...>, "devinfo/..." etc. resolve.
# Only core paths: the core can't reach desktop headers.
target_include_directories(antscopez_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/analyzer
    ${CMAKE_CURRENT_SOURCE_DIR}/analyzer/updater
    ${CMAKE_CURRENT_SOURCE_DIR}/remoteapi
)

target_link_libraries(antscopez_core PUBLIC
    Qt6::Core
    Qt6::SerialPort
    Qt6::Network
    Qt6::Xml
    Qt6::Concurrent
    Qt6::Bluetooth
)

if(WIN32)
    target_sources(antscopez_core PRIVATE analyzer/usbhid/hidapi/windows/hid.c)
    target_link_libraries(antscopez_core PRIVATE setupapi)
    target_include_directories(antscopez_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ftdi)
elseif(APPLE)
    target_sources(antscopez_core PRIVATE analyzer/usbhid/hidapi/mac/hid.c)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    # IOKit -- analyzer/usbhid/hidapi/mac/hid.c calls IOHIDManager* (IOKit/hid/...),
    # which CoreFoundation alone doesn't provide.
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    target_link_libraries(antscopez_core PRIVATE ${COREFOUNDATION_FRAMEWORK} ${IOKIT_FRAMEWORK})
elseif(ANDROID)
    # No libusb on Android: stub hidapi, USB analyzers just never enumerate.
    target_sources(antscopez_core PRIVATE analyzer/usbhid/hidapi/android/hid.c)
else()
    target_sources(antscopez_core PRIVATE analyzer/usbhid/hidapi/linux/hid.c)
    # AppPaths::sharedDataFolder() -- matches the AntScopeZ definition.
    target_compile_definitions(antscopez_core PRIVATE
        ANTSCOPE_SHARED_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/antscopez"
    )
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
    target_link_libraries(antscopez_core PRIVATE PkgConfig::LIBUSB)
endif()
