add_library(e_foc.integration_tests.response STATIC
    PlantTrace.cpp
    PlantTrace.hpp
    ResponseWindow.cpp
    ResponseWindow.hpp
    StepMetrics.hpp
)

target_include_directories(e_foc.integration_tests.response PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.integration_tests.response PUBLIC
    numerical.math
)

add_subdirectory(test)
