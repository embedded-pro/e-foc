add_library(e_foc.services.electrical_system_ident_mock INTERFACE)
emil_build_for(e_foc.services.electrical_system_ident_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.services.electrical_system_ident_mock INTERFACE
    GTest::gmock
    e_foc.services.electrical_system_ident
)

target_sources(e_foc.services.electrical_system_ident_mock INTERFACE
    ElectricalParametersIdentificationMock.hpp
)
