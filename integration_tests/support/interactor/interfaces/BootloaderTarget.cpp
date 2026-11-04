#include "integration_tests/support/interactor/interfaces/BootloaderTarget.hpp"

namespace integration
{
    namespace
    {
        BootloaderTarget* g_instance{ nullptr };
    }

    BootloaderTarget* BootloaderTarget::Instance()
    {
        return g_instance;
    }

    void BootloaderTarget::SetInstance(BootloaderTarget& instance)
    {
        g_instance = &instance;
    }
}
