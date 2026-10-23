#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"
#include "core/foc/interfaces/IsrProfile.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/DutyConversion.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include <algorithm>

namespace foc
{

    SpeedDifferentiator::SpeedDifferentiator(hal::Hertz outerLoopFrequency)
        : samplePeriod{ 1.0f / static_cast<float>(outerLoopFrequency.Value()) }
    {}

    void SpeedDifferentiator::Restart()
    {
        currentAngle = 0.0f;
        windowAngle = 0.0f;
        previousAngle = 0.0f;
        lastSpeed = 0.0f;
        previousAngleValid = false;
    }

    float SpeedDifferentiator::Measure()
    {
        // The window interrupt preempts this reader, so a changed index means the angle may belong to a newer window
        uint32_t index{};
        float angle{};
        do
        {
            index = windowIndex;
            angle = windowAngle;
        } while (index != windowIndex);

        if (!previousAngleValid)
        {
            previousAngle = angle;
            previousIndex = index;
            previousAngleValid = true;
            return 0.0f;
        }

        const auto windows = index - previousIndex;
        if (windows == 0)
            return lastSpeed;

        lastSpeed = detail::PositionWithWrapAround(angle - previousAngle) / (static_cast<float>(windows) * samplePeriod);
        previousAngle = angle;
        previousIndex = index;
        return lastSpeed;
    }

    namespace
    {
        constexpr float excitationCurrentFraction{ 0.025f };
        const hal::Hertz excitationFrequency{ 50 };
        constexpr std::chrono::seconds excitationBurst{ 4 };
        constexpr std::chrono::seconds excitationCycle{ 60 };
    }

    EstimatorWindowAccumulator::EstimatorWindowAccumulator(hal::Hertz tickFrequency, uint32_t ticksPerWindow, uint32_t dutyUpdateDelayPeriods)
        : tickFrequency{ static_cast<float>(tickFrequency.Value()) }
        , ticksPerWindow{ ticksPerWindow }
        , dutyLate{ dutyUpdateDelayPeriods != 0 }
        , mechanical{ ticksPerWindow }
    {
        really_assert(dutyUpdateDelayPeriods <= 1);
    }

    void EstimatorWindowAccumulator::Restart()
    {
        *this = EstimatorWindowAccumulator{ hal::Hertz{ static_cast<uint32_t>(tickFrequency) }, ticksPerWindow, dutyLate ? 1u : 0u };
    }

    DirectAxisExcitation::DirectAxisExcitation(Ampere amplitude, hal::Hertz frequency, hal::Hertz outerLoopFrequency, std::chrono::seconds burst, std::chrono::seconds cycle)
        : amplitude{ amplitude.Value() }
        , halfPeriodSamples{ frequency.Value() != 0 ? std::max<uint32_t>(outerLoopFrequency.Value() / (2 * frequency.Value()), 1) : 0 }
        , burstSamples{ static_cast<uint32_t>(burst.count()) * outerLoopFrequency.Value() }
        , cycleSamples{ std::max<uint32_t>(static_cast<uint32_t>(cycle.count()) * outerLoopFrequency.Value(), 1) }
    {}

    void DirectAxisExcitation::Restart()
    {
        sample = 0;
        cycleSample = 0;
        positive = true;
    }

    float DirectAxisExcitation::Advance()
    {
        if (halfPeriodSamples == 0)
            return 0.0f;

        if (++cycleSample >= cycleSamples)
            Restart();

        if (cycleSample >= burstSamples)
            return 0.0f;

        if (++sample >= halfPeriodSamples)
        {
            sample = 0;
            positive = !positive;
        }

        return positive ? amplitude : -amplitude;
    }

    void EstimatorChannel::SetMechanical(OnlineMechanicalEstimator& estimator)
    {
        mechanical = &estimator;
    }

    void EstimatorChannel::SetElectrical(OnlineElectricalEstimator& estimator)
    {
        electrical = &estimator;
    }

    bool EstimatorChannel::HasElectrical() const
    {
        return electrical != nullptr;
    }

    void EstimatorChannel::UpdateMechanical()
    {
        if (mechanical == nullptr)
            return;

        mechanical->Update(buffer.Acquire().mechanical);
    }

    void EstimatorChannel::UpdateElectrical(float vdcInvScale, Ampere directCurrentReference)
    {
        if (electrical == nullptr)
            return;

        const auto& snapshot = buffer.Acquire();
        electrical->Update(ElectricalWindow{ Volts{ snapshot.meanNormalizedVd * vdcInvScale }, Ampere{ snapshot.meanId }, Ampere{ snapshot.idAtEnd }, snapshot.meanElectricalSpeedTimesIq, directCurrentReference });
    }

    CascadeWithSpeedLoop::CascadeWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency, uint32_t dutyUpdateDelayPeriods)
        : lowPriorityInterrupt(lowPriorityInterrupt)
        , maxCurrent{ maxCurrent }
        , outerLoopFrequency{ lowPriorityFrequency }
        , speedDifferentiator{ lowPriorityFrequency }
        , prescaler{ baseFrequency.Value() / lowPriorityFrequency.Value() }
        , estimatorWindow{ baseFrequency, prescaler, dutyUpdateDelayPeriods }
        , directAxisExcitation{ Ampere{ maxCurrent.Value() * excitationCurrentFraction }, excitationFrequency, lowPriorityFrequency, excitationBurst, excitationCycle }
    {
        really_assert(maxCurrent.Value() > 0);
        really_assert(lowPriorityFrequency.Value() > 0);
        really_assert(lowPriorityFrequency.Value() <= baseFrequency.Value());
        really_assert(baseFrequency.Value() % lowPriorityFrequency.Value() == 0);
    }

    CascadeWithSpeedLoop::~CascadeWithSpeedLoop()
    {
        lowPriorityInterrupt.Unregister();
    }

    bool CascadeWithSpeedLoop::ConfigureImpl(const MotorModelParameters& parameters)
    {
        polePairs = static_cast<float>(parameters.polePairs);
        vdcInvScale = std::numbers::inv_sqrt3_v<float> * parameters.busVoltage.Value();
        return currentLoop.Configure(parameters);
    }

    bool CascadeWithSpeedLoop::ConfigureMechanicsImpl(MechanicalModelParameters& parameters)
    {
        parameters.maxCurrent = maxCurrent;
        parameters.samplingFrequency = outerLoopFrequency;
        return speedLoop.Configure(parameters);
    }

    void CascadeWithSpeedLoop::SetCurrentTuningsImpl(const CurrentLoopTunings& tunings)
    {
        currentLoop.SetTunings(tunings);
    }

    void CascadeWithSpeedLoop::SetSpeedTuningsImpl(const SpeedLoopTunings& tunings)
    {
        speedLoop.SetTunings(tunings);
    }

    void CascadeWithSpeedLoop::EnableSpeedLoop()
    {
        currentLoop.Reset();
        speedLoop.Reset();

        speedDifferentiator.Restart();
        estimatorWindow.Restart();
        directAxisExcitation.Restart();
        lastSpeedLoopOutput = 0.0f;
        directCurrentReference = 0.0f;
        lastElectricalSpeed = 0.0f;
        lastMechanicalSpeed = 0.0f;
        lastMeanIq = 0.0f;
        triggerCounter = 0;
        enabled = true;
    }

    void CascadeWithSpeedLoop::DisableSpeedLoop()
    {
        enabled = false;
        lastElectricalSpeed = 0.0f;
        lastMechanicalSpeed = 0.0f;
        lastMeanIq = 0.0f;
    }

    SelectResult CascadeWithSpeedLoop::SelectCurrentAlgorithmImpl(CurrentAlgorithm algorithm)
    {
        if (enabled)
            return SelectResult::busy;

        return currentLoop.Select(algorithm);
    }

    SelectResult CascadeWithSpeedLoop::SelectSpeedAlgorithmImpl(SpeedAlgorithm algorithm)
    {
        if (enabled)
            return SelectResult::busy;

        return speedLoop.Select(algorithm);
    }

    CurrentAlgorithm CascadeWithSpeedLoop::ActiveCurrentAlgorithmImpl() const
    {
        return currentLoop.Active();
    }

    SpeedAlgorithm CascadeWithSpeedLoop::ActiveSpeedAlgorithmImpl() const
    {
        return speedLoop.Active();
    }

    PhasePwmDutyCycles CascadeWithSpeedLoop::CalculateInnerLoop(const PhaseCurrents& currentPhases, const Radians& position)
    {
        const float ia = currentPhases.a.Value();
        const float ib = currentPhases.b.Value();
        const float ic = currentPhases.c.Value();

        auto mechanicalAngle = position.Value();
        auto electricalAngle = mechanicalAngle * polePairs;
        speedDifferentiator.Track(mechanicalAngle);

        auto cosTheta = FastTrigonometry::Cosine(electricalAngle);
        auto sinTheta = FastTrigonometry::Sine(electricalAngle);

        auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
        diag::isrProfile.Mark(diag::IsrStage::transform);
        const auto coggingCurrent = coggingFeedForward.CurrentAt(electricalAngle);
        const auto quadratureReference = std::clamp(lastSpeedLoopOutput + coggingCurrent, -maxCurrent.Value(), maxCurrent.Value());
        auto voltage = currentLoop.Compute(CurrentControlContext{ idAndIq, RotatingFrame{ directCurrentReference, quadratureReference }, lastElectricalSpeed });
        diag::isrProfile.Mark(diag::IsrStage::currentLoop);

        auto output = spaceVectorModulator.Generate(park.Inverse(voltage, cosTheta, sinTheta));
        diag::isrProfile.Mark(diag::IsrStage::modulation);

        estimatorWindow.Accumulate(idAndIq, voltage, mechanicalAngle, polePairs, coggingCurrent);
        diag::isrProfile.Mark(diag::IsrStage::estimatorWindow);

        ++triggerCounter;
        if (triggerCounter >= prescaler)
        {
            triggerCounter = 0;
            speedDifferentiator.CloseWindow();
            const auto window = estimatorWindow.Close();
            lastMeanIq = window.meanIq;
            estimators.Publish(window);
            lowPriorityInterrupt.Trigger();
            diag::isrProfile.Mark(diag::IsrStage::windowClose);
        }

        return ToDutyCycles(output);
    }

    void CascadeWithSpeedLoop::SetSpeedReference(RadiansPerSecond reference)
    {
        speedReference = reference.Value();
    }

    float CascadeWithSpeedLoop::MeasureMechanicalSpeed()
    {
        const auto mechanicalSpeed = speedDifferentiator.Measure();
        lastMechanicalSpeed = mechanicalSpeed;
        lastElectricalSpeed = mechanicalSpeed * polePairs;
        return mechanicalSpeed;
    }

    void CascadeWithSpeedLoop::RunSpeedLoop(float mechanicalSpeed)
    {
        lastSpeedLoopOutput = speedLoop.Compute(SpeedControlContext{ RadiansPerSecond{ mechanicalSpeed }, RadiansPerSecond{ speedReference } }).Value();
    }

    void CascadeWithSpeedLoop::SetDirectCurrentReference(float current)
    {
        lastSpeedLoopOutput = current;
    }

    float CascadeWithSpeedLoop::CurrentMechanicalAngle() const
    {
        return speedDifferentiator.CurrentAngle();
    }

    MotionObservation CascadeWithSpeedLoop::ObserveSpeedLoopMotion() const
    {
        return { RadiansPerSecond{ lastMechanicalSpeed }, Ampere{ lastMeanIq }, RadiansPerSecond{ speedReference }, Radians{ 0.0f } };
    }

    float CascadeWithSpeedLoop::PolePairs() const
    {
        return polePairs;
    }

    LowPriorityInterrupt& CascadeWithSpeedLoop::GetLowPriorityInterrupt()
    {
        return lowPriorityInterrupt;
    }

    void CascadeWithSpeedLoop::SetOnlineMechanicalEstimatorImpl(OnlineMechanicalEstimator& estimator)
    {
        estimators.SetMechanical(estimator);
    }

    void CascadeWithSpeedLoop::SetOnlineElectricalEstimatorImpl(OnlineElectricalEstimator& estimator)
    {
        estimators.SetElectrical(estimator);
    }

    void CascadeWithSpeedLoop::UpdateOnlineMechanicalEstimator()
    {
        estimators.UpdateMechanical();
    }

    void CascadeWithSpeedLoop::UpdateOnlineElectricalEstimator()
    {
        if (!estimators.HasElectrical())
            return;

        estimators.UpdateElectrical(vdcInvScale, Ampere{ directCurrentReference });
        directCurrentReference = directAxisExcitation.Advance();
    }

    void CascadeWithSpeedLoop::SetDirectAxisExcitation(Ampere amplitude, hal::Hertz frequency)
    {
        directAxisExcitation = DirectAxisExcitation{ amplitude, frequency, outerLoopFrequency, excitationBurst, excitationCycle };
    }

    void CascadeWithSpeedLoop::LoadCoggingMapImpl(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant)
    {
        coggingFeedForward.Load(torque, torqueScale, torqueConstant);
    }

    void CascadeWithSpeedLoop::ClearCoggingMapImpl()
    {
        coggingFeedForward.Clear();
    }
}
