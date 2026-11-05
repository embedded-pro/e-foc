add_library(e_foc.services.bus_voltage ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.bus_voltage PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.bus_voltage PUBLIC
    infra.timer
    infra.util
    e_foc.foc.interfaces
    numerical.filters.passive
)

target_sources(e_foc.services.bus_voltage PRIVATE
    BusVoltageMonitor.cpp
    BusVoltageMonitor.hpp
)

add_subdirectory(test)
