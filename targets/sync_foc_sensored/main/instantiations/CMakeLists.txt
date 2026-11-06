add_library(e_foc.sync_foc_sensored.main.inst ${EMIL_EXCLUDE_FROM_ALL} STATIC)
emil_build_for(e_foc.sync_foc_sensored.main.inst)

target_include_directories(e_foc.sync_foc_sensored.main.inst PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.sync_foc_sensored.main.inst PUBLIC
    e_foc.platform_abstraction
    e_foc.state_machine
    e_foc.can
    e_foc.supervision
    e_foc.foc.cascade
    e_foc.services.cli
    e_foc.services.current_offsets
    e_foc.services.inverter_compensation
    e_foc.services.electrical_system_ident
    e_foc.services.fault_history
    e_foc.services.firmware_upgrade
    e_foc.services.alignment
    e_foc.services.index_search
    e_foc.services.non_volatile_memory
    e_foc.services.standstill
    e_foc.services.bus_voltage
    numerical.controllers.implementations
    services.peripheral
)

add_version_header_dependency(e_foc.sync_foc_sensored.main.inst e_foc.version_header)

target_sources(e_foc.sync_foc_sensored.main.inst PRIVATE
    Logic.cpp
    Logic.hpp
)
