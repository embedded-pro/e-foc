add_executable(e_foc.tool.can_commander)
emil_build_for(e_foc.tool.can_commander HOST All PREREQUISITE_BOOL E_FOC_BUILD_SIMULATOR)

target_include_directories(e_foc.tool.can_commander PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.tool.can_commander PRIVATE
    e_foc.tool.can_commander.adapter
    e_foc.tool.can_commander.logic
    e_foc.tool.can_commander.gui
)

target_sources(e_foc.tool.can_commander PRIVATE
    Main.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.can_commander PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.can_commander PRIVATE
        -Wno-error
    )
endif()
