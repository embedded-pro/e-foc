add_library(e_foc.services.electrical_system_ident ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.electrical_system_ident PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.electrical_system_ident PUBLIC
    infra.util
    e_foc.platform_abstraction
    e_foc.foc.interfaces
    e_foc.foc.implementations
    numerical.estimators.online
)

target_sources(e_foc.services.electrical_system_ident PRIVATE
    ElectricalParametersIdentificationImpl.cpp
    ElectricalParametersIdentificationImpl.hpp
    ElectricalParametersIdentification.hpp
    RealTimeResistanceAndInductanceEstimator.cpp
    RealTimeResistanceAndInductanceEstimator.hpp
    TerminalElectricalParametersIdentification.cpp
    TerminalElectricalParametersIdentification.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
