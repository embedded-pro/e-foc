add_library(e_foc.services.alignment_mock INTERFACE)
emil_build_for(e_foc.services.alignment_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.services.alignment_mock INTERFACE
    gmock_main
    e_foc.services.alignment
)

target_sources(e_foc.services.alignment_mock INTERFACE
    MotorAlignmentMock.hpp
)
