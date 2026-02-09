cmake_minimum_required(VERSION 3.24)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(E_FOC_STANDALONE On)
endif()

option(CMAKE_COMPILE_WARNING_AS_ERROR "Enable warnings-as-error" On)
option(E_FOC_INCLUDE_DEFAULT_INIT "Include default initialization code; turn off when providing custom initialization" On)
option(E_FOC_BUILD_TESTS "Enable build of the examples" Off)

if (E_FOC_BUILD_TESTS)
    set(BUILD_TESTING On)
endif()

set(HALST_INCLUDE_DEFAULT_LINKER_SCRIPTS Off CACHE INTERNAL "")
set(HALST_BUILD_EXAMPLES Off CACHE INTERNAL "")

if(DEFINED EMIL_SEGGER_RTT_BUFFER_SIZE_UP)
    set(SEGGER_RTT_BUFFER_SIZE_UP ${EMIL_SEGGER_RTT_BUFFER_SIZE_UP} CACHE INTERNAL "")
endif()

add_definitions(-DEMIL_ENABLE_TRACING=1)

if(DEFINED E_FOC_TARGET_BOARD)
    add_definitions(-DE_FOC_TARGET_BOARD="${E_FOC_TARGET_BOARD}")
else()
    add_definitions(-DE_FOC_TARGET_BOARD="host")
endif()

project(e_foc LANGUAGES C CXX ASM VERSION 1.0.0) # x-release-please-version

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)

# Use -Og for Debug builds on GCC/Clang to enable optimizations while maintaining debuggability
# This significantly improves FOC real-time performance in Debug builds
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS_DEBUG "-Og -g" CACHE STRING "C Debug flags" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "-Og -g" CACHE STRING "C++ Debug flags" FORCE)
endif()

set_directory_properties(PROPERTY USE_FOLDERS ON)

set(E_FOC_EXCLUDE_FROM_ALL "")

add_subdirectory(embedded-infra-lib)

if (E_FOC_BUILD_TESTS)
    include(CTest)
    emil_enable_testing()
endif()

if (DEFINED TARGET_MCU_VENDOR)
    add_subdirectory(hal/${TARGET_MCU_VENDOR})
    emil_exclude_directory_from_coverage(hal/${TARGET_MCU_VENDOR})
endif()

add_subdirectory(numerical-toolbox)
add_subdirectory(source)

if (E_FOC_BUILD_SIMULATOR)
    add_subdirectory(simulator)
endif()

emil_clangformat_directories(e_foc DIRECTORIES source)
emil_folderize_all_targets()
emil_exclude_directory_from_coverage(embedded-infra-lib)
emil_coverage_targets(DIRECTORIES .)

include(scripts/development_environment.cmake)

if (DEFINED TARGET_MCU)
    generate_development_environment_script(
        TARGET_MCU "${TARGET_MCU}"
        GDB_PORT "2334"
    )
endif()

set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_SOURCE_IGNORE_FILES ".vs/;.git/;build/")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(CPACK_DEBIAN_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_RPM_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

include(CPack)
