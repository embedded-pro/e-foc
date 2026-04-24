find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_subdirectory(model)
add_subdirectory(services)
add_subdirectory(view)
add_subdirectory(position_control)
add_subdirectory(speed_control)
add_subdirectory(torque_control)
