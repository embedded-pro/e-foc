add_library(e_foc.hil_tests.support OBJECT
    BridgeConfig.cpp
    BridgeConfig.hpp
    BridgeSession.cpp
    BridgeSession.hpp
    HilFixture.cpp
    HilFixture.hpp
    GdbFlasher.cpp
    GdbFlasher.hpp
    SerialLogger.cpp
    SerialLogger.hpp
    Timeouts.hpp
    TypingTransmitter.cpp
    TypingTransmitter.hpp
)

target_include_directories(e_foc.hil_tests.support PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.hil_tests.support PUBLIC
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
    services.network_instantiations
    e_foc.tool.hardware_bridge.client
    hal.interfaces
)
