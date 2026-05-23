add_library(e_foc.tool.hardware_bridge.client INTERFACE)

target_link_libraries(e_foc.tool.hardware_bridge.client INTERFACE
    e_foc.tool.hardware_bridge.common
    e_foc.tool.hardware_bridge.serial_client
    e_foc.tool.hardware_bridge.can_client
)
