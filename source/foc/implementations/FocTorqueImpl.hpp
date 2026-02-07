#pragma once

#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "source/foc/implementations/SpaceVectorModulation.hpp"
#include "source/foc/implementations/TransformsClarkePark.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/FieldOrientedController.hpp"

namespace foc
{
    class FocTorqueImpl
        : public FieldOrientedControllerTorqueControl
    {
    public:
        void SetPoint(IdAndIqPoint setPoint) override;
        void SetCurrentTunings(Volts Vdc, const IdAndIqTunings& tunings) override;
        void Reset() override;
        void SetPolePairs(std::size_t polePairs) override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& mechanicalAngle) override;

    private:
        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        controllers::PidIncrementalSynchronous<float> dPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        controllers::PidIncrementalSynchronous<float> qPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        float polePairs{ 1.0f };
    };
}
