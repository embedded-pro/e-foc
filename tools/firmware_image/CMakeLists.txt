# Configured on its own by an embedded build, which runs the tool on the build host (cmake/FirmwareImageTool.cmake)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.24)
    project(e_foc_firmware_image LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED On)

    set(EMIL_BUILD_TESTS Off CACHE BOOL "" FORCE)
    set(EMIL_BUILD_EXAMPLES Off CACHE BOOL "" FORCE)
    set(EMIL_INCLUDE_ECHO Off CACHE BOOL "" FORCE)
    set(EMIL_BUILD_ECHO_COMPILERS Off CACHE BOOL "" FORCE)
    set(EMIL_INCLUDE_UPGRADE Off CACHE BOOL "" FORCE)
    set(EMIL_INCLUDE_MBEDTLS Off CACHE BOOL "" FORCE)
    set(EMIL_INCLUDE_LWIP Off CACHE BOOL "" FORCE)
    set(EMIL_ENABLE_DOCKER_TOOLS Off CACHE BOOL "" FORCE)
    set(EMIL_GENERATE_PACKAGE_CONFIG Off CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../infra/embedded-infra-lib" embedded-infra-lib)
endif()

add_library(e_foc.tool.firmware_image_core STATIC)

target_include_directories(e_foc.tool.firmware_image_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
)

target_link_libraries(e_foc.tool.firmware_image_core PUBLIC
    crypto.micro_ecc
)

target_sources(e_foc.tool.firmware_image_core PRIVATE
    Command.cpp
    Command.hpp
    KeySet.cpp
    KeySet.hpp
)

add_executable(e_foc.tool.firmware_image)

target_link_libraries(e_foc.tool.firmware_image PRIVATE
    e_foc.tool.firmware_image_core
)

target_sources(e_foc.tool.firmware_image PRIVATE
    Main.cpp
)

if(E_FOC_BUILD_TESTS)
    add_subdirectory(test)
endif()
