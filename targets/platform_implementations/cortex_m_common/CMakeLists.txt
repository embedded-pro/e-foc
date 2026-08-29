add_library(e_foc.platform_abstraction_cortex_m_common INTERFACE)

target_include_directories(e_foc.platform_abstraction_cortex_m_common INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.platform_abstraction_cortex_m_common INTERFACE
    e_foc.foc.interfaces
    hal.cortex_m
)
