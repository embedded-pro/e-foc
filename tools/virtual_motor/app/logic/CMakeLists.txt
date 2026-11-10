add_library(e_foc.tool.virtual_motor.app.logic ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_include_directories(e_foc.tool.virtual_motor.app.logic PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
)

target_link_libraries(e_foc.tool.virtual_motor.app.logic PUBLIC
    e_foc.tool.virtual_motor.port
    can_lite.drivers.virtual
)

target_sources(e_foc.tool.virtual_motor.app.logic PRIVATE
    EngineProcesses.hpp
    MotorLink.cpp
    MotorLink.hpp
    MotorText.cpp
    MotorText.hpp
    MotorSpec.cpp
    MotorSpec.hpp
    PortListener.cpp
    PortListener.hpp
    Rig.cpp
    Rig.hpp
)

add_subdirectory(test)
