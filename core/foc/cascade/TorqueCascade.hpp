#pragma once

#include "core/foc/current_loop/CurrentControllerSelector.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/transforms/SpaceVectorModulation.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"

namespace foc
{
    class TorqueCascade
        : public FocTorque
    {
    public:
        void Configure(const MotorModelParameters& parameters) override;
        void SetPoint(IdAndIqPoint setPoint) override;
        void SetCurrentTunings(const CurrentLoopTunings& tunings) override;
        SelectResult SelectCurrentAlgorithm(CurrentAlgorithm algorithm) override;
        CurrentAlgorithm ActiveCurrentAlgorithm() const override;
        void Enable() override;
        void Disable() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) override;

        CurrentControllerSelector& CurrentLoop();

    private:
        float MeasureElectricalSpeed(float mechanicalAngle);

        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        CurrentControllerSelector currentLoop;
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        float polePairs{ 0.0f };
        bool enabled{ false };
        IdAndIqPoint lastSetPoint{ Ampere{ 0.0f }, Ampere{ 0.0f } };
        // Torque mode has no outer loop to measure speed, so the decoupling feedforward is fed
        // from a filtered per-ISR angle difference; raw 20 kHz differences are all encoder quantisation.
        float electricalSpeedScale{ 0.0f };
        float speedFilterAlpha{ 1.0f };
        float previousMechanicalAngle{ 0.0f };
        float electricalSpeed{ 0.0f };
        bool previousAngleValid{ false };
    };
}
