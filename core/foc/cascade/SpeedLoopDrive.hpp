#pragma once

#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"
#include "core/foc/interfaces/Foc.hpp"

namespace foc
{
    template<class FocInterface>
    class SpeedLoopDrive
        : public FocInterface
        , public CurrentCommandable
        , public CoggingCompensable
        , public FrictionCompensable
        , public CurrentLimitable
        , public BusVoltageTrackable
        , protected CascadeWithSpeedLoop
    {
    public:
        bool Configure(const MotorModelParameters& parameters) override
        {
            return ConfigureImpl(parameters);
        }

        void SetCurrentTunings(const CurrentLoopTunings& tunings) override
        {
            SetCurrentTuningsImpl(tunings);
        }

        SelectResult SetSpeedTunings(const SpeedLoopTunings& tunings) override
        {
            return SetSpeedTuningsImpl(tunings);
        }

        SelectResult SelectCurrentAlgorithm(CurrentAlgorithm algorithm) override
        {
            return SelectCurrentAlgorithmImpl(algorithm);
        }

        CurrentAlgorithm ActiveCurrentAlgorithm() const override
        {
            return ActiveCurrentAlgorithmImpl();
        }

        SelectResult SelectSpeedAlgorithm(SpeedAlgorithm algorithm) override
        {
            return SelectSpeedAlgorithmImpl(algorithm);
        }

        SpeedAlgorithm ActiveSpeedAlgorithm() const override
        {
            return ActiveSpeedAlgorithmImpl();
        }

        void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) override
        {
            SetOnlineMechanicalEstimatorImpl(estimator);
        }

        void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) override
        {
            SetOnlineElectricalEstimatorImpl(estimator);
        }

        void CommandCurrent(Ampere quadrature) override
        {
            CommandCurrentImpl(quadrature);
        }

        Volts QuadratureVoltage() const override
        {
            return QuadratureVoltageImpl();
        }

        void LoadCoggingMap(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant) override
        {
            LoadCoggingMapImpl(torque, torqueScale, torqueConstant);
        }

        void ClearCoggingMap() override
        {
            ClearCoggingMapImpl();
        }

        void LoadFrictionFeedForward(NewtonMeter coulomb, NewtonMeter torqueConstant) override
        {
            LoadFrictionFeedForwardImpl(coulomb, torqueConstant);
        }

        void ClearFrictionFeedForward() override
        {
            ClearFrictionFeedForwardImpl();
        }

        void LimitCurrent(Ampere limit) override
        {
            LimitCurrentImpl(limit);
        }

        void TrackBusVoltage(Volts measured) override
        {
            TrackBusVoltageImpl(measured);
        }

        using CascadeWithSpeedLoop::SetCurrentStream;

    protected:
        using CascadeWithSpeedLoop::CascadeWithSpeedLoop;
        ~SpeedLoopDrive() = default;
    };
}
