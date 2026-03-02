add_library(e_foc.sync_foc_sensored.instantiations INTERFACE)

target_include_directories(e_foc.sync_foc_sensored.instantiations INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.sync_foc_sensored.instantiations INTERFACE
    e_foc.foc.instantiations
    e_foc.services.parameter_identification
    e_foc.services.alignment
)

target_sources(e_foc.sync_foc_sensored.instantiations INTERFACE
    MotorStateMachine.hpp
)
