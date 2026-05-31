add_library(e_foc.sync_foc_sensored.main.inst ${EMIL_EXCLUDE_FROM_ALL} STATIC)
emil_build_for(e_foc.sync_foc_sensored.main.inst)

target_include_directories(e_foc.sync_foc_sensored.main.inst PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.sync_foc_sensored.main.inst PUBLIC
    e_foc.platform_abstraction
    e_foc.state_machine
    e_foc.foc.implementations
    e_foc.services.cli
    e_foc.services.electrical_system_ident
    e_foc.services.alignment
    e_foc.services.non_volatile_memory
    can_lite.categories.foc_motor
    numerical.controllers.implementations
    services.peripheral
)

target_sources(e_foc.sync_foc_sensored.main.inst PRIVATE
    Logic.cpp
    Logic.hpp
)
