#pragma once

#include "core/foc/implementations/FocHelpers.hpp"
#include "core/foc/implementations/SpaceVectorModulation.hpp"
#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/implementations/TrigonometricImpl.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class FocWithSpeedLoop
    {
    protected:
        OPTIMIZE_FOR_SPEED
        explicit FocWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
            : speedPid{ { 0.0f, 0.0f, 0.0f }, { -maxCurrent.Value(), maxCurrent.Value() } }
            , lowPriorityInterrupt(lowPriorityInterrupt)
            , dt{ 1.0f / static_cast<float>(baseFrequency.Value()) }
            , speedDt{ 1.0f / static_cast<float>(lowPriorityFrequency.Value()) }
            , prescaler{ baseFrequency.Value() / lowPriorityFrequency.Value() }
        {
            really_assert(maxCurrent.Value() > 0);
            really_assert(lowPriorityFrequency.Value() > 0);
            really_assert(lowPriorityFrequency.Value() <= baseFrequency.Value());
            really_assert(baseFrequency.Value() % lowPriorityFrequency.Value() == 0);

            speedPid.Enable();
            dPid.Enable();
            qPid.Enable();
        }

        OPTIMIZE_FOR_SPEED
        void SetPolePairsImpl(std::size_t pole)
        {
            polePairs = static_cast<float>(pole);
        }

        OPTIMIZE_FOR_SPEED
        void SetCurrentTuningsImpl(Volts Vdc, const IdAndIqTunings& torqueTunings)
        {
            auto scale = 1.0f / (detail::invSqrt3 * Vdc.Value());
            auto scale_dt = scale * dt;
            auto scale_inv_dt = scale / dt;

            const float d_kp = torqueTunings.first.kp;
            const float d_ki = torqueTunings.first.ki;
            const float d_kd = torqueTunings.first.kd;
            const float q_kp = torqueTunings.second.kp;
            const float q_ki = torqueTunings.second.ki;
            const float q_kd = torqueTunings.second.kd;

            dPid.SetTunings({ d_kp * scale, d_ki * scale_dt, d_kd * scale_inv_dt });
            qPid.SetTunings({ q_kp * scale, q_ki * scale_dt, q_kd * scale_inv_dt });
        }

        OPTIMIZE_FOR_SPEED
        void SetSpeedTuningsImpl(const SpeedTunings& speedTuning)
        {
            const float kp = speedTuning.kp;
            const float ki = speedTuning.ki;
            const float kd = speedTuning.kd;

            speedPid.SetTunings({ kp, ki * speedDt, kd / speedDt });
        }

        OPTIMIZE_FOR_SPEED
        void EnableSpeedLoop()
        {
            speedPid.Enable();
            dPid.Enable();
            qPid.Enable();

            currentMechanicalAngle = 0.0f;
            previousSpeedPosition = 0.0f;
            lastSpeedPidOutput = 0.0f;
            triggerCounter = 0;
        }

        OPTIMIZE_FOR_SPEED
        void DisableSpeedLoop()
        {
            speedPid.Disable();
            dPid.Disable();
            qPid.Disable();
        }

        OPTIMIZE_FOR_SPEED
        PhasePwmDutyCycles CalculateInnerLoop(const PhaseCurrents& currentPhases, Radians& position)
        {
            const float ia = currentPhases.a.Value();
            const float ib = currentPhases.b.Value();
            const float ic = currentPhases.c.Value();

            auto mechanicalAngle = position.Value();
            auto electricalAngle = mechanicalAngle * polePairs;
            currentMechanicalAngle = mechanicalAngle;

            auto cosTheta = FastTrigonometry::Cosine(electricalAngle);
            auto sinTheta = FastTrigonometry::Sine(electricalAngle);

            qPid.SetPoint(lastSpeedPidOutput);

            auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
            auto output = spaceVectorModulator.Generate(park.Inverse(RotatingFrame{ dPid.Process(idAndIq.d), qPid.Process(idAndIq.q) }, cosTheta, sinTheta));

            ++triggerCounter;
            if (triggerCounter >= prescaler)
            {
                triggerCounter = 0;
                lowPriorityInterrupt.Trigger();
            }

            return PhasePwmDutyCycles{ hal::Percent(static_cast<uint8_t>(output.a * 100.0f + 0.5f)),
                hal::Percent(static_cast<uint8_t>(output.b * 100.0f + 0.5f)),
                hal::Percent(static_cast<uint8_t>(output.c * 100.0f + 0.5f)) };
        }

    protected:
        controllers::PidIncrementalSynchronous<float>& SpeedPid()
        {
            return speedPid;
        }

        controllers::PidIncrementalSynchronous<float>& DPid()
        {
            return dPid;
        }

        float CurrentMechanicalAngle() const
        {
            return currentMechanicalAngle;
        }

        float& PreviousSpeedPosition()
        {
            return previousSpeedPosition;
        }

        float& LastSpeedPidOutput()
        {
            return lastSpeedPidOutput;
        }

        float SpeedDt() const
        {
            return speedDt;
        }

        LowPriorityInterrupt& GetLowPriorityInterrupt()
        {
            return lowPriorityInterrupt;
        }

    private:
        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        controllers::PidIncrementalSynchronous<float> speedPid;
        controllers::PidIncrementalSynchronous<float> dPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        controllers::PidIncrementalSynchronous<float> qPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        LowPriorityInterrupt& lowPriorityInterrupt;
        float currentMechanicalAngle = 0.0f;
        float previousSpeedPosition = 0.0f;
        float lastSpeedPidOutput = 0.0f;
        float dt;
        float speedDt;
        uint32_t prescaler;
        uint32_t triggerCounter = 0;
        float polePairs = 0.0f;
    };
}
