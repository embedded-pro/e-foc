add_subdirectory(serial_bridge)
add_subdirectory(can_bridge)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_subdirectory(simulator)
add_subdirectory(can_commander)
