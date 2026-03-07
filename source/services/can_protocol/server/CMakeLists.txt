add_library(e_foc.services.can_protocol.server ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.can_protocol.server PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.can_protocol.server PUBLIC
    e_foc.services.can_protocol.core
)

target_sources(e_foc.services.can_protocol.server PRIVATE
    CanCategoryHandlers.cpp
    CanCategoryHandlers.hpp
    CanProtocolServer.hpp
    CanProtocolServerImpl.cpp
    CanProtocolServerImpl.hpp
)

add_subdirectory(test)
