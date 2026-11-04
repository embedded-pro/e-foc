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

set(deviceFamily 1)
set(deviceIdentity 0x0a)

target_compile_definitions(e_foc.platform_abstraction_${E_FOC_TARGET_BOARD} INTERFACE
    E_FOC_BOARD_KIND=${E_FOC_EK_TM4C1294XL_BOARD_KIND}
    E_FOC_DEVICE_FAMILY=${deviceFamily}
    E_FOC_DEVICE_IDENTITY=${deviceIdentity}
    E_FOC_PRODUCT_NAME="${E_FOC_EK_TM4C1294XL_PRODUCT_NAME}"
)

set_property(GLOBAL PROPERTY E_FOC_PRODUCT_NAME "${E_FOC_EK_TM4C1294XL_PRODUCT_NAME}")
set_property(GLOBAL PROPERTY E_FOC_IMAGE_TARGET ${E_FOC_EK_TM4C1294XL_BOARD_KIND} ${deviceFamily} ${deviceIdentity})

set_property(GLOBAL PROPERTY E_FOC_BOOTLOADER_LDSCRIPTS "${CMAKE_CURRENT_LIST_DIR}/ldscripts")
