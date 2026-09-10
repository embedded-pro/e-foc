add_library(e_foc.integration_tests.hooks OBJECT
    Hooks.cpp
)

target_include_directories(e_foc.integration_tests.hooks PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.integration_tests.hooks PUBLIC
    cucumber_cpp.runner
    e_foc.integration_tests.support
    e_foc.integration_tests.interactor.interfaces
)
