#pragma once

#include "core/foc/implementations/SpaceVectorModulation.hpp"
#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"

namespace foc
{
    class FocTorqueImpl
        : public FocTorque
    {
    public:
        void SetPolePairs(std::size_t polePairs) override;
        void SetPoint(IdAndIqPoint setPoint) override;
        void SetCurrentTunings(Volts Vdc, const IdAndIqTunings& tunings) override;
        void Enable() override;
        void Disable() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) override;

    private:
        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        controllers::PidIncrementalSynchronous<float> dPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        controllers::PidIncrementalSynchronous<float> qPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        float polePairs = 0.0f;
        IdAndIqPoint lastSetPoint{ Ampere{ 0.0f }, Ampere{ 0.0f } };
    };
}
