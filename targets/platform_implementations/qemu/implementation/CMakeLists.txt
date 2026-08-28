add_library(e_foc.platform_abstraction_qemu)
emil_build_for(e_foc.platform_abstraction_qemu ALL All BOOL EMIL_BUILD_QEMU)

target_sources(e_foc.platform_abstraction_qemu PRIVATE
    ArrayEeprom.cpp
    ArrayEeprom.hpp
    PlatformFactoryImpl.cpp
    PlatformFactoryImpl.hpp
    SemihostingCan.cpp
    SemihostingCan.hpp
    SemihostingSerial.cpp
    SemihostingSerial.hpp
)

target_include_directories(e_foc.platform_abstraction_qemu PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
)

set(QEMU_MOTOR_MODEL_HEADER "core/foc/model/Jk42bls01X038ed.hpp"
    CACHE STRING "Motor parameter header included by QEMU PlatformFactoryImpl")

target_compile_definitions(e_foc.platform_abstraction_qemu PRIVATE
    QEMU_MOTOR_MODEL_HEADER="${QEMU_MOTOR_MODEL_HEADER}"
)

target_link_libraries(e_foc.platform_abstraction_qemu PUBLIC
    e_foc.platform_abstraction
    e_foc.foc.model
    e_foc.state_machine
    e_foc.can
    can_lite.server
    e_foc.services.alignment
    e_foc.services.electrical_system_ident
    e_foc.services.non_volatile_memory
    hal.cortex_m
    hal.qemu.syscalls
    hal.qemu.sync
    hal.qemu.cortex
    hal.qemu.mps2
    e_foc.platform_abstraction_cortex_m_common
)
