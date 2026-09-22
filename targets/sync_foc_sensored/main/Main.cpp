#include PLATFORM_FACTORY_IMPL_HEADER
#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"
#include <optional>

#ifdef E_FOC_QEMU_TARGET
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#endif

int main()
{
    static std::optional<application::Logic> logic;

    static application::PlatformFactoryImpl hardware(
#ifdef E_FOC_QEMU_TARGET
        foc::M_2310P_LN_04K::parameters,
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
