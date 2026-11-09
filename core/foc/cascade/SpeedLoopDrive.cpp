#include "core/foc/cascade/SpeedLoopDrive.hpp"

namespace foc
{
    template<class FocInterface>
    bool SpeedLoopDrive<FocInterface>::Configure(const MotorModelParameters& parameters)
    {
        return ConfigureImpl(parameters);
    }

    template<class FocInterface>
    void SpeedLoopDrive<FocInterface>::SetCurrentTunings(const CurrentLoopTunings& tunings)
    {
        SetCurrentTuningsImpl(tunings);
    }

    template<class FocInterface>
    SelectResult SpeedLoopDrive<FocInterface>::SetSpeedTunings(const SpeedLoopTunings& tunings)
    {
        return SetSpeedTuningsImpl(tunings);
    }

    template<class FocInterface>
    SelectResult SpeedLoopDrive<FocInterface>::SelectCurrentAlgorithm(CurrentAlgorithm algorithm)
    {
        return SelectCurrentAlgorithmImpl(algorithm);
    }

    template<class FocInterface>
    CurrentAlgorithm SpeedLoopDrive<FocInterface>::ActiveCurrentAlgorithm() const
    {
        return ActiveCurrentAlgorithmImpl();
    }

    template<class FocInterface>
    SelectResult SpeedLoopDrive<FocInterface>::SelectSpeedAlgorithm(SpeedAlgorithm algorithm)
    {
        return SelectSpeedAlgorithmImpl(algorithm);
    }

    template<class FocInterface>
    SpeedAlgorithm SpeedLoopDrive<FocInterface>::ActiveSpeedAlgorithm() const
    {
        return ActiveSpeedAlgorithmImpl();
    }

    template<class FocInterface>
    void SpeedLoopDrive<FocInterface>::SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator)
    {
        SetOnlineMechanicalEstimatorImpl(estimator);
    }

    template<class FocInterface>
    void SpeedLoopDrive<FocInterface>::SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator)
    {
        SetOnlineElectricalEstimatorImpl(estimator);
    }

    template<class FocInterface>
    void SpeedLoopDrive<FocInterface>::CommandCurrent(Ampere quadrature)
    {
        CommandCurrentImpl(quadrature);
    }

    template<class FocInterface>
    Volts SpeedLoopDrive<FocInterface>::QuadratureVoltage() const
    {
        return QuadratureVoltageImpl();
    }

    template<class FocInterface>
    void SpeedLoopDrive<FocInterface>::LoadCoggingMap(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant)
    {
        LoadCoggingMapImpl(torque, torqueScale, torqueConstant);
    }

    template<class FocInterface>
    void SpeedLoopDrive<FocInterface>::ClearCoggingMap()
    {
        ClearCoggingMapImpl();
    }

    template<class FocInterface>
    void SpeedLoopDrive<FocInterface>::LoadFrictionFeedForward(NewtonMeter coulomb, NewtonMeter torqueConstant)
    {
        LoadFrictionFeedForwardImpl(coulomb, torqueConstant);
    }

    template<class FocInterface>
    void SpeedLoopDrive<FocInterface>::ClearFrictionFeedForward()
    {
        ClearFrictionFeedForwardImpl();
    }

    template<class FocInterface>
    void SpeedLoopDrive<FocInterface>::LimitCurrent(Ampere limit)
    {
        LimitCurrentImpl(limit);
    }

    template<class FocInterface>
    void SpeedLoopDrive<FocInterface>::TrackBusVoltage(Volts measured)
    {
        TrackBusVoltageImpl(measured);
    }

    template class SpeedLoopDrive<FocSpeed>;
    template class SpeedLoopDrive<FocPosition>;
}
