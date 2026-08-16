add_library(e_foc.services.controller_selection INTERFACE ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.controller_selection INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.controller_selection INTERFACE
    numerical.math
)

target_sources(e_foc.services.controller_selection PRIVATE
    ControllerSelector.hpp
)
