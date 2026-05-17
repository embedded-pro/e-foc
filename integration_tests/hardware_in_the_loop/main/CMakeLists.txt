add_executable(e_foc.hil_tests)

emil_build_for(e_foc.hil_tests HOST All BOOL E_FOC_BUILD_HIL_TESTS)

target_sources(e_foc.hil_tests PRIVATE
    Main.cpp
)

target_include_directories(e_foc.hil_tests PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.hil_tests PRIVATE
    e_foc.hil_tests.steps
    e_foc.hil_tests.hooks
    e_foc.hil_tests.support
    cucumber_cpp
)
