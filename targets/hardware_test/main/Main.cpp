#include PLATFORM_FACTORY_IMPL_HEADER
#include "targets/hardware_test/instantiations/Logic.hpp"
#include <optional>

#ifdef E_FOC_QEMU_TARGET
#include "motor_parameters/Jk42bls01X038ed.hpp"
#endif

int main()
{
    static std::optional<application::Logic> logic;
    static application::PlatformFactoryImpl hardware(
#ifdef E_FOC_QEMU_TARGET
        foc::JK42BLS01_X038ED::parameters,
#endif
        [&]()
        {
            logic.emplace(hardware);
        });

    hardware.Run();

#if defined(__GNUC__) || defined(__clang__)
    __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(false);
#endif
}
