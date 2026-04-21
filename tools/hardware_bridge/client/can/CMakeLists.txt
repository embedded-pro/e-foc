add_library(e_foc.tool.hardware_bridge.can_client ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.hardware_bridge.can_client PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.hardware_bridge.can_client PUBLIC
    e_foc.tool.hardware_bridge.common
    hal.interfaces
)

target_sources(e_foc.tool.hardware_bridge.can_client PRIVATE
    TcpClientCanbus.hpp
    TcpClientCanbus.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.hardware_bridge.can_client PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.hardware_bridge.can_client PRIVATE
        -Wno-error
    )
endif()

add_subdirectory(test)
