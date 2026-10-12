cmake_minimum_required(VERSION 3.21)

project(FocusLock
    VERSION 1.0.0
    DESCRIPTION "Per-app time budgets with a lock overlay"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "FocusLock uses the Win32 API and only builds on Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

qt_add_executable(FocusLock WIN32
    src/main.cpp
    src/Theme.h
    src/WatchTypes.h
    src/WinUtil.h
    src/WinUtil.cpp
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

target_compile_definitions(FocusLock PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

target_link_libraries(FocusLock PRIVATE
    Qt6::Widgets
    Qt6::Network
    user32
    advapi32
    dwmapi
    version)

if(MSVC)
    target_compile_options(FocusLock PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(FocusLock PRIVATE -Wall -Wextra)
endif()

# Copy the Qt runtime next to the exe so it runs straight from the build folder.
option(FOCUSLOCK_DEPLOY_ON_BUILD "Run windeployqt after each build" ON)
if(FOCUSLOCK_DEPLOY_ON_BUILD)
    if(TARGET Qt6::windeployqt)
        set(_windeployqt "$<TARGET_FILE:Qt6::windeployqt>")
    else()
        find_program(_windeployqt windeployqt HINTS "${QT6_INSTALL_PREFIX}/bin")
    endif()
    if(_windeployqt)
        add_custom_command(TARGET FocusLock POST_BUILD
            COMMAND "${_windeployqt}" --no-translations --no-system-d3d-compiler
                    --no-opengl-sw "$<TARGET_FILE:FocusLock>"
            COMMENT "Deploying Qt runtime next to FocusLock.exe"
            VERBATIM)
    endif()
endif()
