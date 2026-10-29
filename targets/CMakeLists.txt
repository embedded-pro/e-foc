if(NOT E_FOC_TARGET_PLATFORM STREQUAL "host" AND NOT E_FOC_TARGET_PLATFORM STREQUAL "qemu")
    add_subdirectory(platform_implementations/motor_boards)
endif()
if(NOT E_FOC_TARGET_PLATFORM STREQUAL "host")
    add_subdirectory(platform_implementations/cortex_m_common)
endif()
if(E_FOC_TARGET_PLATFORM STREQUAL "host" OR E_FOC_TARGET_PLATFORM STREQUAL "st")
    add_subdirectory(platform_implementations/peripheral_stubs)
endif()
set(E_FOC_PLATFORM_IMPLEMENTATION_DIR implementation)
add_subdirectory(platform_implementations/${E_FOC_TARGET_PLATFORM})
add_subdirectory(platform_implementations/error_handling_cortex_m)

add_library(e_foc.platform_abstraction_impl INTERFACE)

target_compile_definitions(e_foc.platform_abstraction_impl INTERFACE
    PLATFORM_FACTORY_IMPL_HEADER="targets/platform_implementations/${E_FOC_TARGET_PLATFORM}/${E_FOC_PLATFORM_IMPLEMENTATION_DIR}/PlatformFactoryImpl.hpp"
)

target_link_libraries(e_foc.platform_abstraction_impl INTERFACE
    e_foc.platform_abstraction_${E_FOC_TARGET_PLATFORM}
)

add_subdirectory(sync_foc_sensored)
add_subdirectory(hardware_test)
