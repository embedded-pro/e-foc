add_library(e_foc.services.can_protocol.core ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.can_protocol.core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.can_protocol.core PUBLIC
    infra.util
    hal.interfaces
)

target_sources(e_foc.services.can_protocol.core PRIVATE
    CanCategoryHandler.cpp
    CanCategoryHandler.hpp
    CanProtocolDefinitions.hpp
    CanFrameCodec.cpp
    CanFrameCodec.hpp
    CanFrameTransport.cpp
    CanFrameTransport.hpp
)

add_subdirectory(test)
