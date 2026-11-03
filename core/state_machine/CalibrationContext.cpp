#include "core/state_machine/CalibrationContext.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include <algorithm>
#include <numbers>

namespace application
{
    CalibrationContext::CalibrationContext(
        drivers::ThreePhaseInverter& inverter,
        foc::Volts vdc,
        foc::Weber configuredFluxLinkage,
        std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>> currentOffsets,
        uint32_t dutyUpdateDelayPeriods)
        : inverter(inverter)
        , vdc(vdc)
        , configuredFluxLinkage(configuredFluxLinkage)
        , dutyUpdateDelayPeriods(dutyUpdateDelayPeriods)
        , currentOffsets(currentOffsets)
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
        ApplyCurrentOffsets();
    }

    bool CalibrationContext::HasFiniteElectricalParameters(const services::CalibrationData& data)
    {
        return foc::IsFinitePositive(data.rPhase) &&
               foc::IsFiniteValue(data.lD) &&
               foc::IsFiniteValue(data.fluxLinkage) &&
               foc::IsFiniteValue(data.currentLoopBandwidth);
    }

    bool CalibrationContext::IsComplete(bool modeSpecificValid) const
    {
        return calibrationData.stage == services::CalibrationStage::complete &&
               calibrationData.polePairs != 0 &&
               HasFiniteElectricalParameters(calibrationData) &&
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

    bool CalibrationContext::Apply(foc::FocBase& controller, foc::CurrentLoopTunable& tunable)
    {
        ApplyCurrentOffsets();

        return ApplyModel(
            foc::Ohm{ calibrationData.rPhase },
            foc::MilliHenry{ calibrationData.lD },
            calibrationData.polePairs,
            calibrationData.currentLoopBandwidth,
            EffectiveFluxLinkage(),
            foc::Ampere{ services::CurrentNoiseAmpere(calibrationData.currentNoiseMilliAmpere) },
            controller,
            tunable);
    }

    void CalibrationContext::ApplyCurrentOffsets()
    {
        if (currentOffsets)
            currentOffsets->get().Apply(foc::PhaseCurrents{
                foc::Ampere{ calibrationData.currentOffsetA },
                foc::Ampere{ calibrationData.currentOffsetB },
                foc::Ampere{ calibrationData.currentOffsetC } });
    }

    bool CalibrationContext::ApplyModel(
        foc::Ohm resistance,
        foc::MilliHenry inductance,
        std::size_t polePairs,
        float bandwidth,
        foc::Weber fluxLinkage,
        foc::Ampere currentNoise,
        foc::FocBase& controller,
        foc::CurrentLoopTunable& tunable)
    {
        const bool configured = controller.Configure(foc::MotorModelParameters{
            resistance,
            inductance,
            fluxLinkage,
            vdc,
            inverter.BaseFrequency(),
            polePairs,
            dutyUpdateDelayPeriods });

        tunable.SetCurrentTunings(CurrentTuningsFor(bandwidth, inductance, currentNoise));

        return configured;
    }

    foc::Volts CalibrationContext::BusVoltage() const
    {
        return vdc;
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

    float CalibrationContext::DefaultCurrentLoopBandwidth(foc::MilliHenry inductance, foc::Ampere currentNoise) const
    {
        const auto switchingBound = (static_cast<float>(inverter.BaseFrequency().Value()) / nyquistFactor) * 2.0f * std::numbers::pi_v<float>;
        const auto henry = foc::InductanceInHenry(inductance);

        if (!foc::IsFinitePositive(currentNoise.Value()) || !foc::IsFinitePositive(henry))
            return switchingBound;

        const auto noiseBound = std::max(inaudibleVoltageNoise / (henry * currentNoise.Value()), minimumAutomaticBandwidth);
        return std::min(switchingBound, noiseBound);
    }

    foc::CurrentLoopTunings CalibrationContext::CurrentTuningsFor(float bandwidth, foc::MilliHenry inductance, foc::Ampere currentNoise) const
    {
        auto tunings = foc::CurrentLoopTunings{};
        tunings.bandwidth = foc::IsAcceptableCurrentBandwidth(bandwidth) ? bandwidth : DefaultCurrentLoopBandwidth(inductance, currentNoise);
        return tunings;
    }
}
