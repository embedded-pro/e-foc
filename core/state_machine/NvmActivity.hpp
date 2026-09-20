#pragma once

#include <cstdint>

namespace application
{
    class NvmActivity
    {
    public:
        void Begin();
        void End();
        bool InFlight() const;

    private:
        uint8_t outstanding{ 0 };
    };
}
