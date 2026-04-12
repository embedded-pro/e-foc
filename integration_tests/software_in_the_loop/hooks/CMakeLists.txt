add_library(e_foc.integration_tests.hooks OBJECT
    Hooks.cpp
)

target_link_libraries(e_foc.integration_tests.hooks PUBLIC
    cucumber_cpp.runner
)
