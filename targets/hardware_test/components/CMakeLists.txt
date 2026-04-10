add_library(e_foc.hardware_test.components ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.hardware_test.components PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.hardware_test.components PUBLIC
    infra.util
    e_foc.platform_abstraction
    services.util
    services.tracer
    e_foc.foc.implementations
)

target_sources(e_foc.hardware_test.components PRIVATE
    Terminal.cpp
    Terminal.hpp
)

add_subdirectory(test)
