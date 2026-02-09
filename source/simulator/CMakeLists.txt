execute_process(
    COMMAND git config --global url.https://github.com/.insteadOf git@github.com:
    RESULT_VARIABLE GIT_CONFIG_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)

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
target_include_directories(sciplot INTERFACE 
    ${sciplot_SOURCE_DIR}
    OPTIONS 
    -Wno-error           # Disable -Werror for sciplot headers
    -Wno-unused-variable # Disable specific warning
    -Wno-sign-compare    # Disable specific warning
)

add_subdirectory(model)
add_subdirectory(plot)
add_subdirectory(speed_control)
add_subdirectory(torque_control)
