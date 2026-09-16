#include "core/state_machine/CalibrationContext.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include <numbers>

namespace
{
    bool HasFiniteElectricalCalibration(const services::CalibrationData& data)
    {
        return foc::IsFinitePositive(data.rPhase) &&
               foc::IsFiniteValue(data.lD) &&
               foc::IsFiniteValue(data.fluxLinkage) &&
               foc::IsFiniteValue(data.currentLoopBandwidth);
    }
}

namespace application
{
    CalibrationContext::CalibrationContext(
        drivers::ThreePhaseInverter& inverter,
        foc::Volts vdc,
        foc::Weber configuredFluxLinkage)
        : inverter(inverter)
        , vdc(vdc)
        , configuredFluxLinkage(configuredFluxLinkage)
    {}

    const services::CalibrationData& CalibrationContext::Data() const
    {
        return calibrationData;
    }

    services::CalibrationData& CalibrationContext::MutableData()
    {
        return calibrationData;
    }

    void CalibrationContext::SetData(const services::CalibrationData& data)
    {
        calibrationData = data;
    }

    void CalibrationContext::Invalidate()
    {
        calibrationData = services::CalibrationData{};
        rotorReferenceValid_ = false;
    }

    bool CalibrationContext::IsComplete(bool modeSpecificValid) const
    {
        return calibrationData.stage == services::CalibrationStage::complete &&
               calibrationData.polePairs != 0 &&
               HasFiniteElectricalCalibration(calibrationData) &&
               modeSpecificValid;
    }

    bool CalibrationContext::HasPartial() const
    {
        return calibrationData.stage != services::CalibrationStage::complete &&
               (calibrationData.polePairs != 0 || calibrationData.rPhase > 0.0f);
    }

    bool CalibrationContext::IsRotorReferenceValid() const
    {
        return rotorReferenceValid_;
    }

    void CalibrationContext::SetRotorReferenceValid(bool valid)
    {
        rotorReferenceValid_ = valid;
    }

    float CalibrationContext::PendingFluxLinkage() const
    {
        return pendingFluxLinkage;
    }

    void CalibrationContext::SetPendingFluxLinkage(float value)
    {
        pendingFluxLinkage = value;
    }

    void CalibrationContext::CommitPendingFluxLinkage()
    {
        calibrationData.fluxLinkage = pendingFluxLinkage;
    }

    void CalibrationContext::Apply(foc::FocBase& controller, foc::CurrentLoopTunable& tunable)
    {
        ApplyModel(
            foc::Ohm{ calibrationData.rPhase },
            foc::MilliHenry{ calibrationData.lD },
            calibrationData.polePairs,
            calibrationData.currentLoopBandwidth,
            EffectiveFluxLinkage(),
            controller,
            tunable);
    }

    void CalibrationContext::ApplyModel(
        foc::Ohm resistance,
        foc::MilliHenry inductance,
        std::size_t polePairs,
        float bandwidth,
        foc::Weber fluxLinkage,
        foc::FocBase& controller,
        foc::CurrentLoopTunable& tunable)
    {
        controller.Configure(foc::MotorModelParameters{
            resistance,
            inductance,
            fluxLinkage,
            vdc,
            inverter.BaseFrequency(),
            polePairs });

        tunable.SetCurrentTunings(CurrentTuningsFor(bandwidth));
    }

    foc::Weber CalibrationContext::EffectiveFluxLinkage() const
    {
        return EffectiveFluxLinkage(calibrationData);
    }

    foc::Weber CalibrationContext::EffectiveFluxLinkage(const services::CalibrationData& data) const
    {
        return foc::IsFinitePositive(data.fluxLinkage) ? foc::Weber{ data.fluxLinkage } : configuredFluxLinkage;
    }

    foc::Weber CalibrationContext::ActiveFluxLinkage() const
    {
        return EffectiveFluxLinkage();
    }

    float CalibrationContext::DefaultCurrentLoopBandwidth() const
    {
        return (static_cast<float>(inverter.BaseFrequency().Value()) / nyquistFactor) * 2.0f * std::numbers::pi_v<float>;
    }

    foc::CurrentLoopTunings CalibrationContext::CurrentTuningsFor(float bandwidth) const
    {
        auto tunings = foc::CurrentLoopTunings{};
        tunings.bandwidth = foc::IsAcceptableCurrentBandwidth(bandwidth) ? bandwidth : DefaultCurrentLoopBandwidth();
        return tunings;
    }

    drivers::ThreePhaseInverter& CalibrationContext::GetInverter()
    {
        return inverter;
    }

    foc::Volts CalibrationContext::GetVdc() const
    {
        return vdc;
    }
}
