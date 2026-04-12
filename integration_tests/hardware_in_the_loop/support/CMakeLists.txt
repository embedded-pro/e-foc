add_library(e_foc.hil_tests.support OBJECT
    HilFixture.cpp
    HilFixture.hpp
)

target_include_directories(e_foc.hil_tests.support PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.hil_tests.support PUBLIC
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)
