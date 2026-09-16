cmake_minimum_required(VERSION 3.22)
project(REXPlayerLinux VERSION 0.1.0 LANGUAGES CXX)

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
)

target_link_libraries(rex-player PRIVATE
    Qt6::Widgets
    MPV::MPV
)

if(WIN32)
    set_target_properties(rex-player PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

install(TARGETS rex-player RUNTIME DESTINATION bin)
