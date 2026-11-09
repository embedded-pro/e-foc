#include "core/services/cogging_compensation/CoggingMappingImpl.hpp"
#include "core/services/injection_current_limit/InjectionCurrentLimit.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include "numerical/math/FastTrigonometry.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace services
{
    namespace
    {
        constexpr infra::Duration slewPeriod{ std::chrono::milliseconds{ 1 } };
        constexpr float slewPeriodSeconds{ 0.001f };
        constexpr float turn{ 2.0f * std::numbers::pi_v<float> };

        bool IsUsableConfig(const CoggingMapping::Config& config)
        {
            return foc::IsFinitePositive(config.speed.Value()) && foc::IsFinitePositive(config.measuredTurns) &&
                   config.settleTurns >= 0.0f && foc::IsFinitePositive(config.speedSlewRate) &&
                   foc::IsFinitePositive(config.maxCurrent.Value()) && config.timeout > infra::Duration::zero();
        }
    }

    CoggingMappingImpl::CoggingMappingImpl(foc::SpeedCommandable& controller, foc::Controllable& drive, foc::PhaseCurrentsObservable& observable, drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder)
        : controller(controller)
        , drive(drive)
        , observable(observable)
        , inverter(inverter)
        , encoder(encoder)
        , samplePeriod(1.0f / static_cast<float>(inverter.BaseFrequency().Value()))
        , supportedCurrent(inverter.MaxCurrentSupported())
    {}

    void CoggingMappingImpl::Measure(foc::NewtonMeter torqueConstant, foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian viscousFriction, std::size_t polePairs, const Config& config, const infra::Function<void(const CoggingMeasurement&)>& onDone)
    {
        if (IsRunning() || !IsUsableConfig(config) || !foc::IsFinitePositive(torqueConstant.Value()) || !math::IsFinite(inertia.Value()) || inertia.Value() < 0.0f || !math::IsFinite(viscousFriction.Value()) || viscousFriction.Value() < 0.0f || polePairs == 0)
        {
            onDone(CoggingMeasurement{});
            return;
        }

        this->config = config;
        mechanics = Mechanics{ torqueConstant, inertia, viscousFriction, polePairs };
        this->currentEnvelope = foc::Ampere{ std::min(config.maxCurrent.Value(), supportedCurrent.Value() * identificationCurrentMargin) };
        this->onDone = onDone;

        builder.Restart();
        travel.previousPosition = encoder.Read().Value();
        travel.turned = 0.0f;
        travel.mark = 0.0f;
        commandedSpeed = 0.0f;
        ++run;
        phase = Phase::forwardSettle;

        observable.RegisterPhaseCurrentsObserver([this](const auto& currents)
            {
                OnSamplingUpdate(currents);
            });

        drive.Start();
        controller.EnableSpeedCommand();
        controller.CommandSpeed(foc::RadiansPerSecond{ commandedSpeed });

        slewTimer.Start(slewPeriod, [this]()
            {
                StepSpeedCommand();
            });

        timeoutTimer.Start(config.timeout, [this]()
            {
                Finish();
            });
    }

    bool CoggingMappingImpl::IsRunning() const
    {
        return phase != Phase::idle;
    }

    void CoggingMappingImpl::Abort()
    {
        if (!IsRunning())
            return;

        StopMotion();
        phase = Phase::idle;
        onDone = nullptr;
    }

    void CoggingMappingImpl::OnSamplingUpdate(const foc::PhaseCurrents& currentPhases)
    {
        const Phase current = phase;
        if (current == Phase::idle || current == Phase::complete || current == Phase::failed)
            return;

        if (ExceedsInjectionLimit(currentPhases, currentEnvelope))
        {
            phase = Phase::failed;
            inverter.Stop();
            ScheduleFinish();
            return;
        }

        const auto position = encoder.Read().Value();
        const auto rotation = math::WrapToPi(position - travel.previousPosition);
        travel.previousPosition = position;
        travel.turned += rotation;

        const auto electricalAngle = position * static_cast<float>(mechanics.polePairs);
        const auto rotatingFrame = transform.Forward(foc::ThreePhase{ currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() }, math::FastCosine(electricalAngle), math::FastSine(electricalAngle));

        Advance(electricalAngle, rotatingFrame.q, rotation);
    }

    void CoggingMappingImpl::Advance(float electricalAngle, float quadratureCurrent, float rotation)
    {
        switch (phase)
        {
            case Phase::forwardSettle:
                if (travel.turned >= config.settleTurns * turn)
                {
                    travel.mark = travel.turned;
                    phase = Phase::forwardMeasure;
                }
                break;
            case Phase::forwardMeasure:
                builder.Add(CoggingMapBuilder::Direction::forward, electricalAngle, quadratureCurrent, rotation);
                if (travel.turned - travel.mark >= config.measuredTurns * turn)
                    phase = Phase::reversing;
                break;
            case Phase::backwardStart:
                travel.mark = travel.turned;
                phase = Phase::backwardSettle;
                break;
            case Phase::backwardSettle:
                if (travel.mark - travel.turned >= config.settleTurns * turn)
                {
                    travel.mark = travel.turned;
                    phase = Phase::backwardMeasure;
                }
                break;
            case Phase::backwardMeasure:
                builder.Add(CoggingMapBuilder::Direction::backward, electricalAngle, quadratureCurrent, rotation);
                if (travel.mark - travel.turned >= config.measuredTurns * turn)
                {
                    phase = Phase::complete;
                    ScheduleFinish();
                }
                break;
            default:
                break;
        }
    }

    float CoggingMappingImpl::TargetSpeedIn(Phase current) const
    {
        if (current == Phase::forwardSettle || current == Phase::forwardMeasure)
            return config.speed.Value();
        if (current == Phase::reversing || current == Phase::backwardStart || current == Phase::backwardSettle || current == Phase::backwardMeasure)
            return -config.speed.Value();
        return 0.0f;
    }

    void CoggingMappingImpl::StepSpeedCommand()
    {
        const Phase current = phase;
        const auto target = TargetSpeedIn(current);

        const auto step = config.speedSlewRate * slewPeriodSeconds;
        const auto remaining = target - commandedSpeed;
        commandedSpeed = math::Abs(remaining) <= step ? target : commandedSpeed + math::Copysign(step, remaining);
        controller.CommandSpeed(foc::RadiansPerSecond{ commandedSpeed });

        if (current == Phase::reversing && commandedSpeed == target)
            phase = Phase::backwardStart;
    }

    void CoggingMappingImpl::StopMotion()
    {
        slewTimer.Cancel();
        timeoutTimer.Cancel();
        drive.Stop();
        controller.DisableSpeedCommand();
        observable.UnregisterPhaseCurrentsObserver();
    }

    void CoggingMappingImpl::ScheduleFinish()
    {
        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [finishingRun = run](const infra::SharedPtr<CoggingMappingImpl>& self)
            {
                if (self->run != finishingRun)
                    return;

                self->Finish();
            },
            WeakFromThis());
    }

    void CoggingMappingImpl::Finish()
    {
        if (!IsRunning())
            return;

        const bool complete = phase == Phase::complete;
        StopMotion();
        phase = Phase::idle;

        CoggingMeasurement measurement{};
        if (complete)
        {
            measurement.map = builder.Build(mechanics.torqueConstant, mechanics.inertia, mechanics.polePairs, samplePeriod);
            measurement.frictionCoulomb = builder.FrictionCoulomb(mechanics.torqueConstant, mechanics.inertia, mechanics.viscousFriction, mechanics.polePairs, samplePeriod).value_or(foc::NewtonMeter{ 0.0f });
        }

        if (onDone)
            onDone(measurement);
    }
}
