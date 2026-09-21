cmake_minimum_required(VERSION 3.22)
project(REXPlayerLinux VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(WIN32)
    # Windows builds use the MSYS2/MinGW libmpv package. This keeps the
    # Windows build independent of pkg-config and the Linux toolchain.
    find_path(MPV_INCLUDE_DIR
        NAMES mpv/client.h
        PATHS ENV MINGW_PREFIX
        PATH_SUFFIXES include
        REQUIRED
    )
    find_library(MPV_IMPLIB
        NAMES mpv-2 mpv
        PATHS ENV MINGW_PREFIX
        PATH_SUFFIXES lib
        REQUIRED
    )
    find_file(MPV_DLL
        NAMES libmpv-2.dll mpv-2.dll
        PATHS ENV MINGW_PREFIX
        PATH_SUFFIXES bin
        REQUIRED
    )

    add_library(MPV::MPV SHARED IMPORTED)
    set_target_properties(MPV::MPV PROPERTIES
        IMPORTED_IMPLIB "${MPV_IMPLIB}"
        IMPORTED_LOCATION "${MPV_DLL}"
        INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}"
    )
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBMPV REQUIRED IMPORTED_TARGET mpv)
    add_library(MPV::MPV ALIAS PkgConfig::LIBMPV)
endif()

add_executable(rex-player
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    assets/rex-player.qrc
)

if(WIN32)
    # Generate a native multi-resolution Windows icon from the shared REX
    # Player SVG. ImageMagick is supplied by the MSYS2 CI environment.
    set(REX_ICON_SVG "${CMAKE_CURRENT_SOURCE_DIR}/assets/rex-player.svg")
    set(REX_ICON_BINARY "${CMAKE_CURRENT_BINARY_DIR}/rex-player.ico")
    add_custom_command(
        OUTPUT "${REX_ICON_BINARY}"
        COMMAND magick "${REX_ICON_SVG}" -background none
                -define icon:auto-resize=256,128,64,48,32,16
                "${REX_ICON_BINARY}"
        DEPENDS "${REX_ICON_SVG}"
        VERBATIM
    )
    add_custom_target(rex-player-icon DEPENDS "${REX_ICON_BINARY}")
    add_dependencies(rex-player rex-player-icon)

    file(TO_CMAKE_PATH "${REX_ICON_BINARY}" REX_ICON_RC_PATH)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/rex-player.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/rex-player.rc"
        @ONLY
    )
    target_sources(rex-player PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/rex-player.rc"
    )
endif()

target_link_libraries(rex-player PRIVATE
    Qt6::Widgets
    MPV::MPV
)

if(WIN32)
    target_link_libraries(rex-player PRIVATE dwmapi)
endif()

if(WIN32)
    set_target_properties(rex-player PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

install(TARGETS rex-player RUNTIME DESTINATION bin)

if(UNIX AND NOT APPLE)
    install(FILES
        assets/rex-player.desktop
        DESTINATION share/applications
    )
    install(FILES
        assets/rex-player.svg
        DESTINATION share/icons/hicolor/scalable/apps
    )
endif()
