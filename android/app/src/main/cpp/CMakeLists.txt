# ============================================================
# Monero RandomX Miner - Android Native Build
# Location: android/app/src/main/cpp/CMakeLists.txt
# ============================================================
cmake_minimum_required(VERSION 3.22.1)
project("monerominer")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# PATH SETUP
# CMake is at: android/app/src/main/cpp/
# Go up 6 levels to reach project root
# ============================================================
get_filename_component(MINER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)

set(SRC_DIR "${MINER_ROOT}/src")
set(LIBS_DIR "${MINER_ROOT}/libs")
set(RANDOMX_DIR "${LIBS_DIR}/randomx")
set(NLOHMANN_DIR "${LIBS_DIR}/nlohmann")

message(STATUS "=========================================")
message(STATUS "Miner Root: ${MINER_ROOT}")
message(STATUS "Source Dir: ${SRC_DIR}")
message(STATUS "Libs Dir:   ${LIBS_DIR}")
message(STATUS "RandomX:    ${RANDOMX_DIR}")
message(STATUS "=========================================")

# ============================================================
# FIND FILES WITH FALLBACK SEARCH
# ============================================================

# Find src directory
if(NOT EXISTS "${SRC_DIR}/stratum_client.cpp")
    file(GLOB_RECURSE FOUND "${MINER_ROOT}/**/stratum_client.cpp")
    if(FOUND)
        list(GET FOUND 0 FIRST)
        get_filename_component(SRC_DIR "${FIRST}" DIRECTORY)
        message(STATUS "Found src at: ${SRC_DIR}")
    endif()
endif()

# Find libs directory
if(NOT EXISTS "${LIBS_DIR}/randomx_wrapper.cpp")
    file(GLOB_RECURSE FOUND "${MINER_ROOT}/**/randomx_wrapper.cpp")
    if(FOUND)
        list(GET FOUND 0 FIRST)
        get_filename_component(LIBS_DIR "${FIRST}" DIRECTORY)
        message(STATUS "Found libs at: ${LIBS_DIR}")
    endif()
endif()

# Find RandomX
if(NOT EXISTS "${RANDOMX_DIR}/src/randomx.h")
    file(GLOB_RECURSE FOUND "${MINER_ROOT}/**/randomx.h")
    if(FOUND)
        list(GET FOUND 0 FIRST)
        get_filename_component(RX_SRC "${FIRST}" DIRECTORY)
        get_filename_component(RANDOMX_DIR "${RX_SRC}/.." ABSOLUTE)
        message(STATUS "Found RandomX at: ${RANDOMX_DIR}")
    else()
        message(FATAL_ERROR "RandomX NOT FOUND! Clone it to ${RANDOMX_DIR}")
    endif()
endif()

# Find nlohmann/json
if(NOT EXISTS "${NLOHMANN_DIR}/json.hpp")
    file(GLOB_RECURSE FOUND "${MINER_ROOT}/**/json.hpp")
    if(FOUND)
        list(GET FOUND 0 FIRST)
        get_filename_component(NLOHMANN_DIR "${FIRST}" DIRECTORY)
        message(STATUS "Found JSON at: ${NLOHMANN_DIR}")
    endif()
endif()

# ============================================================
# VERIFY REQUIRED FILES
# ============================================================
set(REQUIRED_FILES
    "${SRC_DIR}/stratum_client.cpp"
    "${SRC_DIR}/randomx_miner.cpp"
    "${SRC_DIR}/job_handler.cpp"
    "${LIBS_DIR}/randomx_wrapper.cpp"
    "${RANDOMX_DIR}/src/randomx.h"
)

foreach(FILE ${REQUIRED_FILES})
    if(NOT EXISTS "${FILE}")
        message(FATAL_ERROR "MISSING: ${FILE}")
    endif()
    message(STATUS "  ✅ ${FILE}")
endforeach()

# ============================================================
# SOURCE FILES
# ============================================================
set(SOURCES
    # JNI Bridge (this directory)
    ${CMAKE_CURRENT_SOURCE_DIR}/miner_bridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/android_main.cpp
    # FIX: android_bridge.cpp and battery/thermal helpers were missing from sources
    ${CMAKE_CURRENT_SOURCE_DIR}/android_bridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/android_battery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/android_thermal.cpp

    # Core mining engine
    ${SRC_DIR}/stratum_client.cpp
    ${SRC_DIR}/randomx_miner.cpp
    ${SRC_DIR}/job_handler.cpp

    # RandomX wrapper
    ${LIBS_DIR}/randomx_wrapper.cpp
)

# ============================================================
# RANDOMX LIBRARY
# ============================================================
if(ANDROID AND "${ANDROID_ABI}" STREQUAL "arm64-v8a")
    set(RANDOMX_USE_JIT ON CACHE BOOL "Enable JIT" FORCE)
endif()
add_subdirectory(${RANDOMX_DIR} ${CMAKE_BINARY_DIR}/randomx)

# ============================================================
# CREATE SHARED LIBRARY
# ============================================================
add_library(monerominer SHARED ${SOURCES})

# ============================================================
# INCLUDE DIRECTORIES
# ============================================================
target_include_directories(monerominer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SRC_DIR}
    ${LIBS_DIR}
    ${RANDOMX_DIR}/src
    ${NLOHMANN_DIR}
)

# ============================================================
# LINK LIBRARIES
# ============================================================
find_package(Threads REQUIRED)

target_link_libraries(monerominer
    randomx
    Threads::Threads
    android
    log
)

# ============================================================
# COMPILE DEFINITIONS
# ============================================================
target_compile_definitions(monerominer PRIVATE
    ANDROID
    MOBILE_DEVICE
    NO_HUGEPAGES
    NO_NUMA
)

# ============================================================
# COMPILER OPTIMIZATIONS
# ============================================================
if(ANDROID)
    if("${ANDROID_ABI}" STREQUAL "arm64-v8a")
        target_compile_options(monerominer PRIVATE
            -march=armv8-a
            -mtune=cortex-a76
            -O3
            -fomit-frame-pointer
            -fPIC
            -fvisibility=hidden
            -fdata-sections
            -ffunction-sections
        )
        target_link_options(monerominer PRIVATE
            -Wl,--gc-sections
            -Wl,--strip-all
        )
    elseif("${ANDROID_ABI}" STREQUAL "armeabi-v7a")
        target_compile_options(monerominer PRIVATE
            -march=armv7-a
            -mfpu=neon
            -mfloat-abi=softfp
            -mtune=cortex-a53
            -O3
            -fomit-frame-pointer
            -fPIC
            -fvisibility=hidden
            -fdata-sections
            -ffunction-sections
        )
        target_link_options(monerominer PRIVATE
            -Wl,--gc-sections
            -Wl,--strip-all
        )
    else()
        target_compile_options(monerominer PRIVATE
            -O3
            -fPIC
            -fvisibility=hidden
        )
    endif()
else()
    target_compile_options(monerominer PRIVATE -O3 -fPIC)
endif()

# ============================================================
# STRIP SYMBOLS IN RELEASE
# ============================================================
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(monerominer PROPERTIES
        LINK_FLAGS_RELEASE "-s"
    )
endif()

# ============================================================
# BUILD INFO
# ============================================================
message(STATUS "=========================================")
message(STATUS "BUILD READY")
message(STATUS "  Root:     ${MINER_ROOT}")
message(STATUS "  Src:      ${SRC_DIR}")
message(STATUS "  Libs:     ${LIBS_DIR}")
message(STATUS "  RandomX:  ${RANDOMX_DIR}")
message(STATUS "  JSON:     ${NLOHMANN_DIR}")
message(STATUS "  ABI:      ${ANDROID_ABI}")
message(STATUS "  Build:    ${CMAKE_BUILD_TYPE}")
message(STATUS "=========================================")
