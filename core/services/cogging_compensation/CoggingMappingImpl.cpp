#include "core/services/cogging_compensation/CoggingMappingImpl.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include "core/services/InjectionCurrentLimit.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
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

    void CoggingMappingImpl::Measure(foc::NewtonMeter torqueConstant, foc::NewtonMeterSecondSquared inertia, std::size_t polePairs, const Config& config, const infra::Function<void(std::optional<CoggingMap>)>& onDone)
    {
        if (IsRunning() || !IsUsableConfig(config) || !foc::IsFinitePositive(torqueConstant.Value()) || !foc::IsFiniteValue(inertia.Value()) || inertia.Value() < 0.0f || polePairs == 0)
        {
            onDone(std::nullopt);
            return;
        }

        this->config = config;
        this->torqueConstant = torqueConstant;
        this->inertia = inertia;
        this->polePairs = polePairs;
        this->currentEnvelope = foc::Ampere{ std::min(config.maxCurrent.Value(), supportedCurrent.Value() * identificationCurrentMargin) };
        this->onDone = onDone;

        builder.Restart();
        previousPosition = encoder.Read().Value();
        turned = 0.0f;
        mark = 0.0f;
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
        const auto rotation = foc::detail::PositionWithWrapAround(position - previousPosition);
        previousPosition = position;
        turned += rotation;

        const auto electricalAngle = position * static_cast<float>(polePairs);
        const auto rotatingFrame = transform.Forward(foc::ThreePhase{ currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() }, foc::FastTrigonometry::Cosine(electricalAngle), foc::FastTrigonometry::Sine(electricalAngle));

        Advance(electricalAngle, rotatingFrame.q, rotation);
    }

    void CoggingMappingImpl::Advance(float electricalAngle, float quadratureCurrent, float rotation)
    {
        switch (phase)
        {
            case Phase::forwardSettle:
                if (turned >= config.settleTurns * turn)
                {
                    mark = turned;
                    phase = Phase::forwardMeasure;
                }
                break;
            case Phase::forwardMeasure:
                builder.Add(CoggingMapBuilder::Direction::forward, electricalAngle, quadratureCurrent, rotation);
                if (turned - mark >= config.measuredTurns * turn)
                    phase = Phase::reversing;
                break;
            case Phase::backwardStart:
                mark = turned;
                phase = Phase::backwardSettle;
                break;
            case Phase::backwardSettle:
                if (mark - turned >= config.settleTurns * turn)
                {
                    mark = turned;
                    phase = Phase::backwardMeasure;
                }
                break;
            case Phase::backwardMeasure:
                builder.Add(CoggingMapBuilder::Direction::backward, electricalAngle, quadratureCurrent, rotation);
                if (mark - turned >= config.measuredTurns * turn)
                {
                    phase = Phase::complete;
                    ScheduleFinish();
                }
                break;
            default:
                break;
        }
    }

    void CoggingMappingImpl::StepSpeedCommand()
    {
        const Phase current = phase;
        const bool forward = current == Phase::forwardSettle || current == Phase::forwardMeasure;
        const bool backward = current == Phase::reversing || current == Phase::backwardStart || current == Phase::backwardSettle || current == Phase::backwardMeasure;
        const auto target = forward ? config.speed.Value() : (backward ? -config.speed.Value() : 0.0f);

        const auto step = config.speedSlewRate * slewPeriodSeconds;
        const auto remaining = target - commandedSpeed;
        commandedSpeed = std::abs(remaining) <= step ? target : commandedSpeed + std::copysign(step, remaining);
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

        const auto map = complete ? builder.Build(torqueConstant, inertia, polePairs, samplePeriod) : std::nullopt;

        if (onDone)
            onDone(map);
    }
}
