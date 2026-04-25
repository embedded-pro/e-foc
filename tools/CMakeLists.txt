find_package(Qt6 REQUIRED COMPONENTS Network Widgets)
qt_standard_project_setup()

add_subdirectory(hardware_bridge)
add_subdirectory(simulator)
add_subdirectory(can_commander)
