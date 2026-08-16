add_library(e_foc.foc.selection INTERFACE ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.selection INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.selection INTERFACE
    numerical.math
)

target_sources(e_foc.foc.selection INTERFACE
    ControllerSelector.hpp
)
