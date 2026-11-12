add_library(e_foc.integration_tests.interactor.virtual_motor OBJECT)

target_include_directories(e_foc.integration_tests.interactor.virtual_motor PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
)

target_link_libraries(e_foc.integration_tests.interactor.virtual_motor PUBLIC
    e_foc.integration_tests.interactor.interfaces
    can_lite.drivers.codec
    can_lite.drivers.virtual
    hal.interfaces
    GTest::gtest
)

if(UNIX)
    target_sources(e_foc.integration_tests.interactor.virtual_motor PRIVATE
        VirtualMotorInteractor.hpp
        VirtualMotorInteractor.cpp
    )
endif()
