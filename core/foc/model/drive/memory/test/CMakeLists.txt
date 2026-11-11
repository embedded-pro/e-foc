add_executable(e_foc.foc.model.drive.memory_test)
emil_build_for(e_foc.foc.model.drive.memory_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc.model.drive.memory_test)

target_link_libraries(e_foc.foc.model.drive.memory_test PUBLIC
    gmock_main
    e_foc.foc.model.drive.memory
)

target_sources(e_foc.foc.model.drive.memory_test PRIVATE
    TestNvmImageBuilder.cpp
)
