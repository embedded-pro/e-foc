add_subdirectory(platform_implementations/${E_FOC_TARGET_PLATFORM})

add_library(e_foc.platform_abstraction_impl INTERFACE)

target_compile_definitions(e_foc.platform_abstraction_impl INTERFACE
    PLATFORM_FACTORY_IMPL_HEADER="targets/platform_implementations/${E_FOC_TARGET_PLATFORM}/implementation/PlatformFactoryImpl.hpp"
)

target_link_libraries(e_foc.platform_abstraction_impl INTERFACE
    e_foc.platform_abstraction_${E_FOC_TARGET_PLATFORM}
)

add_subdirectory(sync_foc_sensored)
add_subdirectory(hardware_test)
