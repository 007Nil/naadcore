# Build piano plugin as shared library

cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth>=2.0)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${FLUIDSYNTH_INCLUDE_DIRS})

# Create piano plugin as SHARED library
add_library(piano_plugin SHARED
    ${CMAKE_SOURCE_DIR}/plugins/piano/piano_plugin.cpp
)

# SoundFont path (can be overridden via -DPIANO_SOUNDFONT_PATH=... during cmake)
# Default: the in-repo GeneralUser GS font — portable, fully self-contained
# (the plugin never reads a font outside this repository).
if(NOT PIANO_SOUNDFONT_PATH)
    set(PIANO_SOUNDFONT_PATH
        "${CMAKE_SOURCE_DIR}/plugins/piano/soundfonts/GeneralUserGS.sf2")
endif()

target_compile_definitions(piano_plugin PRIVATE
    PIANO_SOUNDFONT_PATH="${PIANO_SOUNDFONT_PATH}"
)

target_link_libraries(piano_plugin
    naadcore_core
    ${FLUIDSYNTH_LIBRARIES}
)

set_target_properties(piano_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    POSITION_INDEPENDENT_CODE ON
)

# Install rules
install(TARGETS piano_plugin
    LIBRARY DESTINATION lib/naadcore/plugins
)
