add_library(e_foc.integration_tests.interactor.hardware OBJECT
    BridgeConfig.hpp
    BridgeConfig.cpp
    BridgeSession.hpp
    BridgeSession.cpp
    GdbFlasher.hpp
    GdbFlasher.cpp
    HardwareInteractor.hpp
    HardwareInteractor.cpp
    SerialLogger.hpp
    SerialLogger.cpp
    Timeouts.hpp
    TypingTransmitter.hpp
    TypingTransmitter.cpp
)

target_include_directories(e_foc.integration_tests.interactor.hardware PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
)

target_link_libraries(e_foc.integration_tests.interactor.hardware PUBLIC
    e_foc.integration_tests.interactor.interfaces
    e_foc.tool.hardware_bridge.client
    services.network_instantiations
    hal.interfaces
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)
