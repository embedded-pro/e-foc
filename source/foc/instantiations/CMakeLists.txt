add_library(e_foc.foc.instantiations ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.instantiations PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.instantiations PUBLIC
    e_foc.foc.implementations
    numerical.controllers.implementations
)

target_sources(e_foc.foc.instantiations PRIVATE
    FocImpl.cpp
    FocImpl.hpp
    TrigonometricImpl.cpp
    TrigonometricImpl.hpp
)
