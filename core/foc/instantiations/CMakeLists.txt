add_library(e_foc.foc.instantiations INTERFACE)

target_include_directories(e_foc.foc.instantiations INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.instantiations INTERFACE
    e_foc.foc.implementations
)

target_sources(e_foc.foc.instantiations INTERFACE
    FocController.hpp
)
