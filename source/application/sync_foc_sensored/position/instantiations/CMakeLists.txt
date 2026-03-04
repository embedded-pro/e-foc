add_library(e_foc.sync_foc_sensored.position.inst ${EMIL_EXCLUDE_FROM_ALL} STATIC)
emil_build_for(e_foc.sync_foc_sensored.position.inst)

target_include_directories(e_foc.sync_foc_sensored.position.inst PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.sync_foc_sensored.position.inst PUBLIC
    e_foc.hardware
    e_foc.services.cli
    e_foc.sync_foc_sensored.instantiations
    numerical.controllers.implementations
)

target_sources(e_foc.sync_foc_sensored.position.inst PRIVATE
    Logic.cpp
    Logic.hpp
)
