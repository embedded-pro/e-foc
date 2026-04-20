add_library(e_foc.tool.serial_bridge.lib ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.serial_bridge.lib PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.serial_bridge.lib PUBLIC
    hal.interfaces
    services.network.connection
)

target_sources(e_foc.tool.serial_bridge.lib PRIVATE
    TcpClientSerial.hpp
    TcpClientSerial.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.serial_bridge.lib PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.serial_bridge.lib PRIVATE
        -Wno-error
    )
endif()

add_subdirectory(test)
