#pragma once

#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "numerical/math/TrigonometricFunctions.hpp"
#include "source/foc/implementations/SpaceVectorModulation.hpp"
#include "source/foc/implementations/TransformsClarkePark.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/Foc.hpp"

namespace foc
{
    class FocTorqueImpl
        : public FocTorque
    {
    public:
        explicit FocTorqueImpl(math::TrigonometricFunctions<float>& trigFunctions);

        void SetPolePairs(std::size_t polePairs) override;
        void SetPoint(IdAndIqPoint setPoint) override;
        void SetCurrentTunings(Volts Vdc, const IdAndIqTunings& tunings) override;
        void Enable() override;
        void Disable() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) override;
        hal::Hertz BaseFrequency() const override;

    private:
        math::TrigonometricFunctions<float>& trigFunctions;
        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        controllers::PidIncrementalSynchronous<float> dPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        controllers::PidIncrementalSynchronous<float> qPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        float polePairs;
    };

    class FocSpeedImpl
        : public FocSpeed
    {
    public:
        explicit FocSpeedImpl(math::TrigonometricFunctions<float>& trigFunctions, foc::Ampere maxCurrent, std::chrono::system_clock::duration timeStep);

        void SetPolePairs(std::size_t polePairs) override;
        void SetPoint(RadiansPerSecond point) override;
        void SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings) override;
        void SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning) override;
        void Enable() override;
        void Disable() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) override;
        hal::Hertz BaseFrequency() const override;

    private:
        float CalculateFilteredSpeed(float currentPosition);

    private:
        math::TrigonometricFunctions<float>& trigFunctions;
        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        controllers::PidIncrementalSynchronous<float> speedPid;
        controllers::PidIncrementalSynchronous<float> dPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        controllers::PidIncrementalSynchronous<float> qPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        float previousPosition = 0.0f;
        float dt;
        float polePairs;
    };
}
