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
# Default: the in-repo derived font (Phase 6) — portable, fully self-contained
# (the plugin never reads a font outside this repository). It carries preset 0
# "harmonium" (identical to the original font), preset 1 "harmonium double"
# (2-reed detuned layering) and preset 2 "key click" (the self-ending chiff
# layer used by the key_click config key; see tests/scripts/derive_sf2.py and
# docs/HARMONIUM_SF2_AUDIT.md). harmonium_v2.sf2 (Phase 3, without the click
# preset) and harmonium_original.sf2 (the committed upstream provenance copy,
# input to derive_sf2.py) also live in soundfonts/ — see soundfonts/README.md.
if(NOT HARMONIUM_SOUNDFONT_PATH)
    set(HARMONIUM_SOUNDFONT_PATH
        "${CMAKE_SOURCE_DIR}/plugins/harmonium/soundfonts/harmonium_v3.sf2")
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
