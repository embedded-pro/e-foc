add_library(e_foc.tool.can_commander.adapter ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.can_commander.adapter PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.can_commander.adapter PUBLIC
    infra.util
)

target_sources(e_foc.tool.can_commander.adapter PRIVATE
    CanBusAdapter.hpp
    SocketCanAdapter.hpp
    SocketCanAdapter.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.can_commander.adapter PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.can_commander.adapter PRIVATE
        -Wno-error
    )
endif()
