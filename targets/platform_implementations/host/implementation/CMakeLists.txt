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
)

target_sources(e_foc.platform_abstraction_${E_FOC_TARGET_PLATFORM} PRIVATE
    HardwareFactoryImpl.cpp
    HardwareFactoryImpl.hpp
)
