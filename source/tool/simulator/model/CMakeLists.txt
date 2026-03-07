add_library(e_foc.tool.simulator.model ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.simulator.model PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.simulator.model PUBLIC
    infra.util
    numerical.math
    e_foc.foc.implementations
)

target_sources(e_foc.tool.simulator.model PRIVATE
    Model.hpp
    Model.cpp
)
