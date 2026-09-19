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

# Build identifier for the CLI startup banner ("naadcore-cli build <id>").
# The id is the short commit hash at configure time (fallback "unknown" when
# git is unavailable), plus the configure date, so a stale binary is
# detectable at a glance. The date comes from CMake (string TIMESTAMP), not
# the compiler's __DATE__ macro — splicing __DATE__ through a -D define
# triggers a "-Wbuiltin-macro-redefined" warning.
set(NAADCORE_BUILD_GIT_ID "unknown")
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE NAADCORE_BUILD_GIT_ID_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE NAADCORE_BUILD_GIT_RESULT
)
if(NAADCORE_BUILD_GIT_RESULT EQUAL 0 AND NOT NAADCORE_BUILD_GIT_ID_RAW STREQUAL "")
    set(NAADCORE_BUILD_GIT_ID "${NAADCORE_BUILD_GIT_ID_RAW}")
endif()

# Create naadcore-cli executable
add_executable(naadcore-cli
    ${CMAKE_SOURCE_DIR}/apps/naadcore-cli/main.cpp
)

string(TIMESTAMP NAADCORE_BUILD_DATE "%Y-%m-%d")

target_compile_definitions(naadcore-cli PRIVATE
    NAADCORE_BUILD_ID="${NAADCORE_BUILD_GIT_ID} ${NAADCORE_BUILD_DATE}"
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
