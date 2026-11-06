#pragma once

#include "core/platform_abstraction/interfaces/Drivers.hpp"

namespace application
{
    class NoEncoderIndex
        : public drivers::EncoderIndex
    {
    public:
        bool HasIndex() const override
        {
            return false;
        }

        void ArmIndex() override
        {}

        bool IndexSeen() override
        {
            return false;
        }
    };
}
