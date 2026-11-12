add_library(e_foc.services.fault_history ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.fault_history PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.fault_history PUBLIC
    infra.timer
    infra.util
    e_foc.foc.interfaces
    e_foc.services.non_volatile_memory
)

target_sources(e_foc.services.fault_history PRIVATE
    FaultHistory.cpp
    FaultHistory.hpp
    FaultLog.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
