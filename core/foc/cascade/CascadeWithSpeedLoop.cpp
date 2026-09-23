#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"
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
        previousAngleValid = false;
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
    OPTIMIZE_FOR_SPEED
    float SpeedDifferentiator::Measure()
    {
        const float angle = windowAngle;

        if (!previousAngleValid)
        {
            previousAngle = angle;
            previousAngleValid = true;
            return 0.0f;
        }

        const auto speed = detail::PositionWithWrapAround(angle - previousAngle) / samplePeriod;
        previousAngle = angle;
        return speed;
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

    namespace
    {
        constexpr float excitationCurrentFraction{ 0.025f };
        const hal::Hertz excitationFrequency{ 10 };
    }

    EstimatorWindowAccumulator::EstimatorWindowAccumulator(hal::Hertz tickFrequency)
        : tickFrequency{ static_cast<float>(tickFrequency.Value()) }
    {}

    void EstimatorWindowAccumulator::Restart()
    {
        *this = EstimatorWindowAccumulator{ hal::Hertz{ static_cast<uint32_t>(tickFrequency) } };
    }

    DirectAxisExcitation::DirectAxisExcitation(Ampere amplitude, hal::Hertz frequency, hal::Hertz outerLoopFrequency)
        : amplitude{ amplitude.Value() }
        , halfPeriodSamples{ frequency.Value() != 0 ? std::max<uint32_t>(outerLoopFrequency.Value() / (2 * frequency.Value()), 1) : 0 }
    {}

    void DirectAxisExcitation::Restart()
    {
        sample = 0;
        positive = true;
    }

    float DirectAxisExcitation::Advance()
    {
        if (halfPeriodSamples == 0)
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

    void EstimatorChannel::UpdateMechanical(float mechanicalSpeed)
    {
        if (mechanical == nullptr)
            return;

        const auto& snapshot = Acquire();
        mechanical->Update(MechanicalWindow{ Ampere{ snapshot.meanIq }, RadiansPerSecond{ mechanicalSpeed } });
    }

    void EstimatorChannel::UpdateElectrical(float vdcInvScale)
    {
        if (electrical == nullptr)
            return;

        const auto& snapshot = Acquire();
        electrical->Update(ElectricalWindow{ Volts{ snapshot.meanNormalizedVd * vdcInvScale }, Ampere{ snapshot.meanId }, Ampere{ snapshot.idAtEnd }, snapshot.meanElectricalSpeedTimesIq });
    }

    CascadeWithSpeedLoop::CascadeWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
        : lowPriorityInterrupt(lowPriorityInterrupt)
        , maxCurrent{ maxCurrent }
        , outerLoopFrequency{ lowPriorityFrequency }
        , speedDifferentiator{ lowPriorityFrequency }
        , prescaler{ baseFrequency.Value() / lowPriorityFrequency.Value() }
        , estimatorWindow{ baseFrequency }
        , directAxisExcitation{ Ampere{ maxCurrent.Value() * excitationCurrentFraction }, excitationFrequency, lowPriorityFrequency }
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

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
    OPTIMIZE_FOR_SPEED
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
        auto voltage = currentLoop.Compute(CurrentControlContext{ idAndIq, RotatingFrame{ directCurrentReference, lastSpeedLoopOutput }, lastElectricalSpeed });

        auto output = spaceVectorModulator.Generate(park.Inverse(voltage, cosTheta, sinTheta));

        estimatorWindow.Accumulate(idAndIq, voltage, mechanicalAngle, polePairs);

        ++triggerCounter;
        if (triggerCounter >= prescaler)
        {
            triggerCounter = 0;
            speedDifferentiator.CloseWindow();
            const auto window = estimatorWindow.Close();
            lastMeanIq = window.meanIq;
            estimators.Publish(window);
            lowPriorityInterrupt.Trigger();
        }

        return ToDutyCycles(output);
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

    void CascadeWithSpeedLoop::SetSpeedReference(RadiansPerSecond reference)
    {
        speedReference = reference.Value();
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
    OPTIMIZE_FOR_SPEED
    float CascadeWithSpeedLoop::MeasureMechanicalSpeed()
    {
        const auto mechanicalSpeed = speedDifferentiator.Measure();
        lastMechanicalSpeed = mechanicalSpeed;
        lastElectricalSpeed = mechanicalSpeed * polePairs;
        return mechanicalSpeed;
    }

    OPTIMIZE_FOR_SPEED
    void CascadeWithSpeedLoop::RunSpeedLoop(float mechanicalSpeed)
    {
        lastSpeedLoopOutput = speedLoop.Compute(SpeedControlContext{ RadiansPerSecond{ mechanicalSpeed }, RadiansPerSecond{ speedReference } }).Value();
    }

    OPTIMIZE_FOR_SPEED
    void CascadeWithSpeedLoop::SetDirectCurrentReference(float current)
    {
        lastSpeedLoopOutput = current;
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

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

    void CascadeWithSpeedLoop::UpdateOnlineMechanicalEstimator(float mechanicalSpeed)
    {
        estimators.UpdateMechanical(mechanicalSpeed);
    }

    void CascadeWithSpeedLoop::UpdateOnlineElectricalEstimator()
    {
        if (!estimators.HasElectrical())
            return;

        estimators.UpdateElectrical(vdcInvScale);
        directCurrentReference = directAxisExcitation.Advance();
    }

    void CascadeWithSpeedLoop::SetDirectAxisExcitation(Ampere amplitude, hal::Hertz frequency)
    {
        directAxisExcitation = DirectAxisExcitation{ amplitude, frequency, outerLoopFrequency };
    }
}
