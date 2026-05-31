add_library(e_foc.hil_tests.hooks OBJECT
    Hooks.cpp
)

target_include_directories(e_foc.hil_tests.hooks PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.hil_tests.hooks PUBLIC
    cucumber_cpp.runner
    e_foc.hil_tests.support
)
