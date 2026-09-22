add_library(e_foc.services.injection_current_limit INTERFACE ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.injection_current_limit INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.injection_current_limit INTERFACE
    e_foc.foc.interfaces
)

target_sources(e_foc.services.injection_current_limit PRIVATE
    InjectionCurrentLimit.hpp
)

add_subdirectory(alignment)
add_subdirectory(cli)
add_subdirectory(electrical_system_ident)
add_subdirectory(mechanical_system_ident)
add_subdirectory(non_volatile_memory)
add_subdirectory(test)
