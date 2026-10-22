add_library(e_foc.services.cogging_compensation ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.cogging_compensation PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.cogging_compensation PUBLIC
    infra.event
    infra.timer
    infra.util
    e_foc.platform_abstraction
    e_foc.foc.interfaces
    e_foc.foc.math
    e_foc.foc.transforms
    e_foc.services.injection_current_limit
    e_foc.services.mechanical_system_ident
    e_foc.services.non_volatile_memory
    numerical.math
)

target_sources(e_foc.services.cogging_compensation PRIVATE
    CoggingMap.hpp
    CoggingMapBuilder.cpp
    CoggingMapBuilder.hpp
    CoggingMapping.hpp
    CoggingMappingImpl.cpp
    CoggingMappingImpl.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
