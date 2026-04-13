add_library(e_foc.error_handling_cortex_m)

target_include_directories(e_foc.error_handling_cortex_m PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.error_handling_cortex_m PUBLIC
    e_foc.platform_abstraction
    infra.stream
)

target_sources(e_foc.error_handling_cortex_m PRIVATE
    PersistentFaultData.hpp
    PersistentFaultData.cpp
)

add_subdirectory(test)
