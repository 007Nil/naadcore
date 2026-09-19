cmake_minimum_required(VERSION 3.16)
project(naadcore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth>=2.0)
pkg_check_modules(ALSA REQUIRED alsa)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${FLUIDSYNTH_INCLUDE_DIRS})
include_directories(${ALSA_INCLUDE_DIRS})

# Create core library (SHARED for plugins)
add_library(naadcore_core SHARED
    ${CMAKE_SOURCE_DIR}/core/midi.cpp
    ${CMAKE_SOURCE_DIR}/core/plugin_manager.cpp
)
target_link_libraries(naadcore_core
    ${FLUIDSYNTH_LIBRARIES}
    ${ALSA_LIBRARIES}
)

# Create naadcore-cli executable (plugin loader)
add_subdirectory(${CMAKE_SOURCE_DIR}/apps/naadcore-cli)

# Create harmonium plugin
add_subdirectory(${CMAKE_SOURCE_DIR}/plugins/harmonium)

# Create piano plugin
add_subdirectory(${CMAKE_SOURCE_DIR}/plugins/piano)

# Install rules
install(TARGETS naadcore-cli
    RUNTIME DESTINATION bin
)

install(TARGETS naadcore_core harmonium_plugin piano_plugin
    LIBRARY DESTINATION lib/naadcore
)
