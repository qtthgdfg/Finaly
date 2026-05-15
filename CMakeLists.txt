# FILE: MoneroRandomXMiner/CMakeLists.txt
# This is ONLY for desktop builds
# Android builds use android/app/src/main/cpp/CMakeLists.txt

cmake_minimum_required(VERSION 3.15)
project(MoneroRandomXMiner VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_WITH_SSL "Enable SSL/TLS support" ON)
option(BUILD_WITH_HWLOC "Enable NUMA/hwloc support" ON)
option(ENABLE_HUGEPAGES "Enable huge pages support" ON)

# Find packages
find_package(Threads REQUIRED)

if(BUILD_WITH_SSL)
    find_package(OpenSSL REQUIRED)
endif()

if(BUILD_WITH_HWLOC)
    find_package(hwloc)
endif()

# Source files - Desktop only
set(SOURCES
    src/main.cpp
    src/stratum_client.cpp
    src/randomx_miner.cpp
    src/job_handler.cpp
    libs/randomx_wrapper.cpp
)

set(HEADERS
    src/stratum_client.h
    src/randomx_miner.h
    src/job_handler.h
    libs/randomx_wrapper.h
)

# RandomX library
set(RANDOMX_DIR "${CMAKE_SOURCE_DIR}/libs/randomx")
add_subdirectory(${RANDOMX_DIR} ${CMAKE_BINARY_DIR}/randomx)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
    ${RANDOMX_DIR}/src
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    randomx
)

if(BUILD_WITH_SSL)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        OpenSSL::SSL 
        OpenSSL::Crypto
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_SSL)
endif()

if(BUILD_WITH_HWLOC AND hwloc_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE hwloc::hwloc)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_HWLOC)
endif()

# Compiler optimizations - Desktop
target_compile_options(${PROJECT_NAME} PRIVATE
    -O3
    -march=native
    -mtune=native
    -funroll-loops
    -fomit-frame-pointer
    -flto
)

set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

# Platform specific
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${PROJECT_NAME} PRIVATE LINUX)
    if(ENABLE_HUGEPAGES)
        target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_HUGEPAGES)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(${PROJECT_NAME} PRIVATE WINDOWS)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${PROJECT_NAME} PRIVATE MACOS)
endif()

# Install
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES config.json DESTINATION etc/${PROJECT_NAME})
