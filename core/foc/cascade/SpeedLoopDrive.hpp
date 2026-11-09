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
        bool Configure(const MotorModelParameters& parameters) override;
        void SetCurrentTunings(const CurrentLoopTunings& tunings) override;
        SelectResult SetSpeedTunings(const SpeedLoopTunings& tunings) override;
        SelectResult SelectCurrentAlgorithm(CurrentAlgorithm algorithm) override;
        CurrentAlgorithm ActiveCurrentAlgorithm() const override;
        SelectResult SelectSpeedAlgorithm(SpeedAlgorithm algorithm) override;
        SpeedAlgorithm ActiveSpeedAlgorithm() const override;
        void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) override;
        void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) override;
        void CommandCurrent(Ampere quadrature) override;
        Volts QuadratureVoltage() const override;
        void LoadCoggingMap(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant) override;
        void ClearCoggingMap() override;
        void LoadFrictionFeedForward(NewtonMeter coulomb, NewtonMeter torqueConstant) override;
        void ClearFrictionFeedForward() override;
        void LimitCurrent(Ampere limit) override;
        void TrackBusVoltage(Volts measured) override;

        using CascadeWithSpeedLoop::SetCurrentStream;

    protected:
        using CascadeWithSpeedLoop::CascadeWithSpeedLoop;
        ~SpeedLoopDrive() = default;
    };

    extern template class SpeedLoopDrive<FocSpeed>;
    extern template class SpeedLoopDrive<FocPosition>;
}
