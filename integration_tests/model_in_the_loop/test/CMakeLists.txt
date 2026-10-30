add_executable(e_foc.model_in_the_loop_test)
emil_build_for(e_foc.model_in_the_loop_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.model_in_the_loop_test)

target_link_libraries(e_foc.model_in_the_loop_test PUBLIC
    gmock_main
    e_foc.model_in_the_loop
)

target_sources(e_foc.model_in_the_loop_test PRIVATE
    TestSupport.hpp
    TestControlPerformance.cpp
    TestControlRobustness.cpp
    TestCurrentStream.cpp
    TestDisturbanceRejection.cpp
    TestIdentificationRobustness.cpp
    TestParameterIdentification.cpp
)
