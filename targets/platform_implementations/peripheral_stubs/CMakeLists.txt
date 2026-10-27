add_library(e_foc.platform_abstraction.peripheral_stubs STATIC)

target_include_directories(e_foc.platform_abstraction.peripheral_stubs PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.platform_abstraction.peripheral_stubs PUBLIC
    hal.interfaces
    hal.synchronous_interfaces
)

target_sources(e_foc.platform_abstraction.peripheral_stubs PRIVATE
    PeripheralStubs.cpp
    PeripheralStubs.hpp
)
