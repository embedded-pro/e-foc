add_library(e_foc.services.index_search ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.index_search PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.index_search PUBLIC
    infra.util
    infra.timer
    e_foc.platform_abstraction
    e_foc.foc.interfaces
    e_foc.foc.math
    e_foc.foc.transforms
    e_foc.services.injection_current_limit
)

target_sources(e_foc.services.index_search PRIVATE
    IndexSearch.hpp
    IndexSearchImpl.cpp
    IndexSearchImpl.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
