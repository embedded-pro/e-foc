add_library(e_foc.services.current_offsets_mock INTERFACE)
emil_build_for(e_foc.services.current_offsets_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.services.current_offsets_mock INTERFACE
    gmock_main
    e_foc.services.current_offsets
)

target_sources(e_foc.services.current_offsets_mock INTERFACE
    PhaseCurrentOffsetsMock.hpp
)
