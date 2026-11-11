#pragma once

#include "tools/virtual_motor/app/logic/EngineProcesses.hpp"

namespace virtual_motor::app
{
    class QtRigStore
        : public RigStore
    {
    public:
        std::vector<MotorSpec> Load() override;
        void Save(const std::vector<MotorSpec>& motors) override;
    };
}
