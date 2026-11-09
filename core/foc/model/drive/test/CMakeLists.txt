add_executable(e_foc.foc.model.drive_test)
emil_build_for(e_foc.foc.model.drive_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc.model.drive_test)

target_link_libraries(e_foc.foc.model.drive_test PUBLIC
    gmock_main
    e_foc.foc.model.drive
)

target_sources(e_foc.foc.model.drive_test PRIVATE
    TestPlantDrive.cpp
)
