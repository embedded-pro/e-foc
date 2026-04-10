add_library(e_foc.services.non_volatile_memory_mock INTERFACE)
emil_build_for(e_foc.services.non_volatile_memory_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.services.non_volatile_memory_mock INTERFACE
    gmock_main
    e_foc.services.non_volatile_memory
)

target_sources(e_foc.services.non_volatile_memory_mock INTERFACE
    NonVolatileMemoryMock.hpp
)
