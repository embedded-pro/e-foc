#pragma once

#include "core/foc/implementations/FocWithSpeedLoop.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"

namespace foc
{
    class FocPositionImpl
        : public FocPosition
        , protected FocWithSpeedLoop
    {
    public:
        explicit FocPositionImpl(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency = hal::Hertz{ 1000 });

        void SetPolePairs(std::size_t polePairs) override;
        void SetPoint(Radians point) override;
        void SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings) override;
        void SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning) override;
        void SetPositionTunings(const PositionTunings& positionTuning) override;
        void Enable() override;
        void Disable() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) override;

    private:
        void LowPriorityHandler();

    private:
        controllers::PidIncrementalSynchronous<float> positionPid{ { 0.0f, 0.0f, 0.0f }, { -1000.0f, 1000.0f } };
        Radians lastPositionSetPoint{ 0.0f };
    };
}
