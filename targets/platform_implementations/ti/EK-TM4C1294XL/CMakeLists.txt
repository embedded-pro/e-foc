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

target_compile_definitions(e_foc.platform_abstraction_${E_FOC_TARGET_BOARD} INTERFACE
    E_FOC_BOARD_KIND=1
    E_FOC_DEVICE_FAMILY=1
    E_FOC_DEVICE_IDENTITY=0x0a
    E_FOC_PRODUCT_NAME="e-foc EK-TM4C1294XL TM4C1294NCPDT"
)

set_property(GLOBAL PROPERTY E_FOC_BOOTLOADER_LDSCRIPTS "${CMAKE_CURRENT_LIST_DIR}/ldscripts")
