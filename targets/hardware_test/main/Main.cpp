#include PLATFORM_FACTORY_IMPL_HEADER
#include "infra/util/StaticStorage.hpp"
#include "targets/hardware_test/instantiations/Logic.hpp"

#ifdef E_FOC_QEMU_TARGET
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#endif

int main()
{
    // Never destroyed, so none of their destructors is linked: the event loop does not return
    static infra::StaticStorage<application::Logic> logic;
    static infra::StaticStorage<application::PlatformFactoryImpl> hardware;

    hardware.Construct(
#ifdef E_FOC_QEMU_TARGET
        foc::M_2310P_LN_04K::parameters,
#endif
        []()
        {
            logic.Construct(*hardware);
        });

    hardware->Run();

#if defined(__GNUC__) || defined(__clang__)
    __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(false);
#endif
}
