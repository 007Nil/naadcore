# Build harmonium plugin as shared library

cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth>=2.0)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${FLUIDSYNTH_INCLUDE_DIRS})

# Create harmonium plugin as SHARED library
add_library(harmonium_plugin SHARED
    ${CMAKE_SOURCE_DIR}/plugins/harmonium/harmonium_plugin.cpp
)

# SoundFont path (can be overridden via -DHARMONIUM_SOUNDFONT_PATH=... during cmake)
# Default: look for harmonium.sf2 in the project root
if(NOT HARMONIUM_SOUNDFONT_PATH)
    set(HARMONIUM_SOUNDFONT_PATH "/home/nil/harmonium-companion/harmonium.sf2")
endif()

target_compile_definitions(harmonium_plugin PRIVATE
    HARMONIUM_SOUNDFONT_PATH="${HARMONIUM_SOUNDFONT_PATH}"
)

target_link_libraries(harmonium_plugin
    naadcore_core
    ${FLUIDSYNTH_LIBRARIES}
)

set_target_properties(harmonium_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    POSITION_INDEPENDENT_CODE ON
)

# Install rules
install(TARGETS harmonium_plugin
    LIBRARY DESTINATION lib/naadcore/plugins
)
