add_library(e_foc.services.current_offsets ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.current_offsets PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.current_offsets PUBLIC
    infra.event
    infra.timer
    infra.util
    e_foc.platform_abstraction
    e_foc.foc.interfaces
    e_foc.services.injection_current_limit
)

target_sources(e_foc.services.current_offsets PRIVATE
    PhaseCurrentOffsetCompensation.cpp
    PhaseCurrentOffsetCompensation.hpp
    PhaseCurrentOffsets.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
