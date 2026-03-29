add_library(e_foc.hardware_test.instantiations ${EMIL_EXCLUDE_FROM_ALL} STATIC)
emil_build_for(e_foc.hardware_test.instantiations)

target_include_directories(e_foc.hardware_test.instantiations PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.hardware_test.instantiations PUBLIC
    e_foc.hardware
    e_foc.hardware_test.components
    e_foc.foc.implementations
    e_foc.services.cli
    numerical.controllers.implementations
    services.peripheral
)

target_sources(e_foc.hardware_test.instantiations PRIVATE
    Logic.cpp
    Logic.hpp
)
