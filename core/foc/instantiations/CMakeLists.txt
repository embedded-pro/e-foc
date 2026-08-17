add_library(e_foc.foc.instantiations ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.instantiations PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.instantiations PUBLIC
    e_foc.foc.cascade
    e_foc.platform_abstraction.interfaces
    infra.event
)

target_sources(e_foc.foc.instantiations PRIVATE
    FocController.hpp
    LowPriorityInterruptImpl.hpp
    Runner.cpp
    Runner.hpp
)

add_subdirectory(test)
