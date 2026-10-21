add_library(e_foc.services.dead_time_compensation ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.dead_time_compensation PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.dead_time_compensation PUBLIC
    infra.util
    e_foc.platform_abstraction.interfaces
    e_foc.foc.interfaces
    numerical.math
)

target_sources(e_foc.services.dead_time_compensation PRIVATE
    DeadTimeCompensation.cpp
    DeadTimeCompensation.hpp
)

add_subdirectory(test)
