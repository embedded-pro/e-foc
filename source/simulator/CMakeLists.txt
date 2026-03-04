include(FetchContent)

FetchContent_Declare(
    sciplot
    GIT_REPOSITORY          https://github.com/sciplot/sciplot.git
    GIT_TAG                 v0.2.2
    GIT_SHALLOW             TRUE
    GIT_SUBMODULES_RECURSE  TRUE
)

FetchContent_GetProperties(sciplot)
if(NOT sciplot_POPULATED)
    FetchContent_Populate(sciplot)
endif()

add_library(sciplot INTERFACE)
target_include_directories(sciplot SYSTEM INTERFACE
    ${sciplot_SOURCE_DIR}
)
target_compile_options(sciplot INTERFACE
    -Wno-error
    -Wno-unused-variable
    -Wno-sign-compare
)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_subdirectory(model)
add_subdirectory(view)
add_subdirectory(headless)
add_subdirectory(position_control)
add_subdirectory(speed_control)
add_subdirectory(torque_control)
