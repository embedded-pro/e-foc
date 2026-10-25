#include "integration_tests/support/interactor/interfaces/TerminalInteractor.hpp"

namespace integration
{
    namespace
    {
        TerminalInteractor* g_instance{ nullptr };
    }

    TerminalInteractor* TerminalInteractor::Instance()
    {
        return g_instance;
    }

    void TerminalInteractor::SetInstance(TerminalInteractor& instance)
    {
        g_instance = &instance;
    }
}
