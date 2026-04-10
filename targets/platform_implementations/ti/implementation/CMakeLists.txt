add_library(e_foc.platform_abstraction_${E_FOC_TARGET_PLATFORM})

target_include_directories(e_foc.platform_abstraction_${E_FOC_TARGET_PLATFORM} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.platform_abstraction_${E_FOC_TARGET_PLATFORM} PUBLIC
    e_foc.platform_abstraction
    infra.event
    infra.timer
    services.tracer
    e_foc.platform_abstraction_${E_FOC_TARGET_BOARD}
)

target_compile_definitions(e_foc.platform_abstraction_${E_FOC_TARGET_PLATFORM} PUBLIC
    HARDWARE_PINS_AND_PERIPHERALS_HEADER="targets/platform_implementations/${E_FOC_TARGET_PLATFORM}/${E_FOC_TARGET_BOARD}/PinsAndPeripherals.hpp"
)

target_sources(e_foc.platform_abstraction_${E_FOC_TARGET_PLATFORM} PRIVATE
    PlatformFactoryImpl.cpp
    PlatformFactoryImpl.hpp
)
