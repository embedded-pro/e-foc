#include "core/foc/implementations/FocWithSpeedLoop.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    OPTIMIZE_FOR_SPEED
    FocWithSpeedLoop::FocWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
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
    void FocWithSpeedLoop::SetPolePairsImpl(std::size_t pole)
    {
        polePairs = static_cast<float>(pole);
    }

    OPTIMIZE_FOR_SPEED
    void FocWithSpeedLoop::SetCurrentTuningsImpl(Volts Vdc, const IdAndIqTunings& torqueTunings)
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

        vdcInvScale = detail::invSqrt3 * Vdc.Value();
    }

    OPTIMIZE_FOR_SPEED
    void FocWithSpeedLoop::SetSpeedTuningsImpl(const SpeedTunings& speedTuning)
    {
        const float kp = speedTuning.kp;
        const float ki = speedTuning.ki;
        const float kd = speedTuning.kd;

        speedPid.SetTunings({ kp, ki * speedDt, kd / speedDt });
    }

    OPTIMIZE_FOR_SPEED
    void FocWithSpeedLoop::EnableSpeedLoop()
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
    void FocWithSpeedLoop::DisableSpeedLoop()
    {
        speedPid.Disable();
        dPid.Disable();
        qPid.Disable();
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles FocWithSpeedLoop::CalculateInnerLoop(const PhaseCurrents& currentPhases, const Radians& position)
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
        float normalizedVd = dPid.Process(idAndIq.d);
        float normalizedVq = qPid.Process(idAndIq.q);

        auto output = spaceVectorModulator.Generate(park.Inverse(RotatingFrame{ normalizedVd, normalizedVq }, cosTheta, sinTheta));

        ++triggerCounter;
        if (triggerCounter >= prescaler)
        {
            triggerCounter = 0;
            const uint8_t writeSlot = 1u - readyIndex;
            snapshots[writeSlot] = EstimatorSnapshot{ currentPhases, electricalAngle, idAndIq.d, idAndIq.q, normalizedVd };
            readyIndex = writeSlot;
            lowPriorityInterrupt.Trigger();
        }

        return PhasePwmDutyCycles{ hal::Percent(static_cast<uint8_t>(output.a * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.b * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.c * 100.0f + 0.5f)) };
    }

    controllers::PidIncrementalSynchronous<float>& FocWithSpeedLoop::SpeedPid()
    {
        return speedPid;
    }

    controllers::PidIncrementalSynchronous<float>& FocWithSpeedLoop::DPid()
    {
        return dPid;
    }

    float FocWithSpeedLoop::CurrentMechanicalAngle() const
    {
        return currentMechanicalAngle;
    }

    float& FocWithSpeedLoop::PreviousSpeedPosition()
    {
        return previousSpeedPosition;
    }

    float& FocWithSpeedLoop::LastSpeedPidOutput()
    {
        return lastSpeedPidOutput;
    }

    float FocWithSpeedLoop::SpeedDt() const
    {
        return speedDt;
    }

    float FocWithSpeedLoop::PolePairs() const
    {
        return polePairs;
    }

    LowPriorityInterrupt& FocWithSpeedLoop::GetLowPriorityInterrupt()
    {
        return lowPriorityInterrupt;
    }

    void FocWithSpeedLoop::SetOnlineMechanicalEstimatorImpl(OnlineMechanicalEstimator& estimator)
    {
        onlineMechEstimator = &estimator;
    }

    void FocWithSpeedLoop::SetOnlineElectricalEstimatorImpl(OnlineElectricalEstimator& estimator)
    {
        onlineElecEstimator = &estimator;
    }

    void FocWithSpeedLoop::UpdateOnlineMechanicalEstimator(float mechanicalSpeed)
    {
        if (onlineMechEstimator == nullptr)
            return;

        const auto& snapshot = snapshots[readyIndex];
        onlineMechEstimator->Update(
            snapshot.phaseCurrents,
            RadiansPerSecond{ mechanicalSpeed },
            Radians{ snapshot.electricalAngle });
    }

    void FocWithSpeedLoop::UpdateOnlineElectricalEstimator(float electricalSpeed)
    {
        if (onlineElecEstimator == nullptr)
            return;

        const auto& snapshot = snapshots[readyIndex];
        const float physicalVd = snapshot.normalizedVd * vdcInvScale;
        onlineElecEstimator->Update(
            Volts{ physicalVd },
            Ampere{ snapshot.measuredId },
            Ampere{ snapshot.measuredIq },
            RadiansPerSecond{ electricalSpeed });
    }
}
