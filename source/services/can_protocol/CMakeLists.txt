add_library(e_foc.services.can_protocol ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.can_protocol PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.can_protocol PUBLIC
    infra.util
    hal.interfaces
)

target_sources(e_foc.services.can_protocol PRIVATE
    CanCategoryHandler.cpp
    CanCategoryHandler.hpp
    CanCategoryHandlers.cpp
    CanCategoryHandlers.hpp
    CanFrameCodec.cpp
    CanFrameCodec.hpp
    CanProtocolDefinitions.hpp
    CanProtocolHandler.hpp
    CanProtocolHandlerImpl.cpp
    CanProtocolHandlerImpl.hpp
    CanProtocolHandlerObserver.hpp
)

add_subdirectory(test)
