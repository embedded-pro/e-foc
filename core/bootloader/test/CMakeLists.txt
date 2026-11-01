add_executable(e_foc.bootloader_test)
emil_build_for(e_foc.bootloader_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.bootloader_test)

target_link_libraries(e_foc.bootloader_test PUBLIC
    gmock_main
    e_foc.bootloader
)

target_sources(e_foc.bootloader_test PRIVATE
    FlashFake.hpp
    TestStatusJournal.cpp
)
