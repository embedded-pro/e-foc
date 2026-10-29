add_library(e_foc.services.flux_linkage_ident ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.flux_linkage_ident PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.flux_linkage_ident PUBLIC
    infra.timer
    infra.util
    e_foc.platform_abstraction
    e_foc.foc.interfaces
    e_foc.foc.math
)

target_sources(e_foc.services.flux_linkage_ident PRIVATE
    BackEmfFit.hpp
    FluxLinkageIdentification.hpp
    FluxLinkageIdentificationImpl.cpp
    FluxLinkageIdentificationImpl.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
