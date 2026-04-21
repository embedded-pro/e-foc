add_library(e_foc.tool.hardware_bridge.common ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.hardware_bridge.common PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.hardware_bridge.common PUBLIC
    infra.util
    services.network.connection
    services.tracer
)

target_sources(e_foc.tool.hardware_bridge.common PRIVATE
    TcpClientBase.hpp
    TcpClientBase.cpp
    BridgeException.hpp
)

if(MSVC)
    target_compile_options(e_foc.tool.hardware_bridge.common PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.hardware_bridge.common PRIVATE
        -Wno-error
    )
endif()
