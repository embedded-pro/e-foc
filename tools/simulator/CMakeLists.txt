find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_subdirectory(model)
add_subdirectory(view)
add_subdirectory(app)
