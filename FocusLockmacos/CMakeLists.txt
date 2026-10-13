cmake_minimum_required(VERSION 3.21)

# Qt 6.8 itself needs macOS 12 or later.
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(FocusLock
    VERSION 1.0.0
    DESCRIPTION "Per-app time budgets with a lock overlay"
    LANGUAGES CXX OBJCXX)

if(NOT APPLE)
    message(FATAL_ERROR "This is the macOS port of FocusLock. Build the Windows version from the folder above.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

qt_add_executable(FocusLock MACOSX_BUNDLE
    src/main.cpp
    src/Theme.h
    src/WatchTypes.h
    src/MacUtil.h
    src/MacUtil.mm
    src/TrustedClock.h
    src/TrustedClock.cpp
    src/ProcessMonitor.h
    src/ProcessMonitor.cpp
    src/OverlayDialog.h
    src/OverlayDialog.cpp
    src/WatchCard.h
    src/WatchCard.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

set_target_properties(FocusLock PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.focuslock.FocusLock"
    MACOSX_BUNDLE_BUNDLE_NAME "FocusLock"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

target_link_libraries(FocusLock PRIVATE
    Qt6::Widgets
    Qt6::Network
    "-framework AppKit"
    "-framework CoreGraphics")

target_compile_options(FocusLock PRIVATE
    -Wall -Wextra
    $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)

# Bundle the Qt frameworks into FocusLock.app so it runs on Macs without Qt.
# Not needed to run it from the build folder on the Mac that built it.
option(FOCUSLOCK_DEPLOY_ON_BUILD "Run macdeployqt after each build" OFF)
if(FOCUSLOCK_DEPLOY_ON_BUILD)
    if(TARGET Qt6::macdeployqt)
        set(_macdeployqt "$<TARGET_FILE:Qt6::macdeployqt>")
    else()
        find_program(_macdeployqt macdeployqt HINTS "${QT6_INSTALL_PREFIX}/bin")
    endif()
    if(_macdeployqt)
        add_custom_command(TARGET FocusLock POST_BUILD
            COMMAND "${_macdeployqt}" "$<TARGET_BUNDLE_DIR:FocusLock>"
            COMMENT "Bundling the Qt frameworks into FocusLock.app"
            VERBATIM)
    endif()
endif()
