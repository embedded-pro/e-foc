add_executable(e_foc.services.non_volatile_memory_test)
emil_build_for(e_foc.services.non_volatile_memory_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.non_volatile_memory_test)

target_link_libraries(e_foc.services.non_volatile_memory_test PUBLIC
    gmock_main
    e_foc.services.non_volatile_memory
    hal.interfaces_test_doubles
    infra.event_test_helper
)

target_sources(e_foc.services.non_volatile_memory_test PRIVATE
    TestNonVolatileMemory.cpp
    TestNvmFlashRegion.cpp
)
