add_library(e_foc.foc.interfaces INTERFACE ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.interfaces INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.interfaces INTERFACE
    infra.util
    numerical.math
    numerical.controllers.interfaces
)

target_sources(e_foc.foc.interfaces PRIVATE
    Driver.hpp
    Foc.hpp
    Units.hpp
)
