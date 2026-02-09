add_library(e_foc.simulator.pmsm ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.simulator.pmsm PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.simulator.pmsm PUBLIC
    infra.util
    numerical.math
    numerical.controllers.implementations
    e_foc.foc.instantiations
)

target_sources(e_foc.simulator.pmsm PRIVATE
    Model.hpp
    Model.cpp
)
