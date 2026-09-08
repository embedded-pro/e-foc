add_library(e_foc.integration_tests.interactor.qemu OBJECT)

target_include_directories(e_foc.integration_tests.interactor.qemu PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
)

target_link_libraries(e_foc.integration_tests.interactor.qemu PUBLIC
    e_foc.integration_tests.interactor.interfaces
    hal.interfaces
)

if(UNIX)
    target_sources(e_foc.integration_tests.interactor.qemu PRIVATE
        QemuInteractor.hpp
        QemuInteractor.cpp
        QemuSilSession.hpp
        QemuSilSession.cpp
        EepromStub.hpp
        EepromStub.cpp
    )
endif()
