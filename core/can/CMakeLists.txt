add_library(e_foc.can STATIC)

target_include_directories(e_foc.can PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.can PUBLIC
    can_lite.core
    can_lite.server
    can_lite.client
    can_lite.tracing_protocol
    can_lite.tracing
    e_foc.state_machine
    services.tracer
)

target_sources(e_foc.can PRIVATE
    FocMotorMessages.hpp
    FocMotorCategoryServer.hpp
    FocMotorCategoryServer.cpp
    FocMotorCategoryClient.hpp
    FocMotorCategoryClient.cpp
    FocMotorCanBridge.hpp
    FocMotorCanBridge.cpp
    FocMotorCanClient.hpp
    FocMotorCanClient.cpp
    CanLivenessWatchdog.hpp
    CanLivenessWatchdog.cpp
)

add_subdirectory(test)
