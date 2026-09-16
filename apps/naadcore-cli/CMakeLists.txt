# Build CLI loader application

cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth>=2.0)
pkg_check_modules(ALSA REQUIRED alsa)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${FLUIDSYNTH_INCLUDE_DIRS})
include_directories(${ALSA_INCLUDE_DIRS})

# Create naadcore-cli executable
add_executable(naadcore-cli
    ${CMAKE_SOURCE_DIR}/apps/naadcore-cli/main.cpp
)

target_link_libraries(naadcore-cli
    naadcore_core
    ${FLUIDSYNTH_LIBRARIES}
    ${ALSA_LIBRARIES}
)

# Install rules
install(TARGETS naadcore-cli
    RUNTIME DESTINATION bin
)
