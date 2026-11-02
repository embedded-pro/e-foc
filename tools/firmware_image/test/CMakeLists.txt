add_executable(e_foc.tool.firmware_image_test)
emil_build_for(e_foc.tool.firmware_image_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.firmware_image_test)

target_link_libraries(e_foc.tool.firmware_image_test PUBLIC
    gmock_main
    e_foc.tool.firmware_image_core
)

target_sources(e_foc.tool.firmware_image_test PRIVATE
    TestCommand.cpp
    TestKeySet.cpp
)
