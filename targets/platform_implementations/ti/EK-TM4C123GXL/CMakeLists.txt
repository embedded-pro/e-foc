add_library(e_foc.platform_abstraction_${E_FOC_TARGET_BOARD} INTERFACE)

target_include_directories(e_foc.platform_abstraction_${E_FOC_TARGET_BOARD} INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.platform_abstraction_${E_FOC_TARGET_BOARD} INTERFACE
    hal_tiva.tiva
    hal_tiva.synchronous_tiva
)

target_sources(e_foc.platform_abstraction_${E_FOC_TARGET_BOARD} INTERFACE
    PinsAndPeripherals.hpp
)
