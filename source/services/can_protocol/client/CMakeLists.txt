add_library(e_foc.services.can_protocol.client ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.can_protocol.client PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(e_foc.services.can_protocol.client PRIVATE
    CanProtocolClient.hpp
    CanProtocolClientImpl.cpp
    CanProtocolClientImpl.hpp
)

target_link_libraries(e_foc.services.can_protocol.client PUBLIC
    e_foc.services.can_protocol.core
)

add_subdirectory(test)
