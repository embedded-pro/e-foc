#include "core/foc/model/drive/PlantEvents.hpp"
#include <algorithm>
#include <optional>

namespace sil
{
    namespace
    {
        constexpr uint32_t millisecondsPerSecond = 1000;

        std::optional<uint32_t> TicksAfter(bool scheduled, uint64_t milliseconds, uint32_t baseHz)
        {
            if (!scheduled)
                return std::nullopt;

            return static_cast<uint32_t>(milliseconds * baseHz / millisecondsPerSecond);
        }
    }

    foc::PlantResponseSample PlantSampleAt(foc::ThreePhaseMotorModel& model, uint32_t tick)
    {
        const auto& mechanics = model.Mechanics();
        const auto dq = model.Electrical().LastDqCurrents();
        return foc::PlantResponseSample{ tick, mechanics.Speed().Value(), mechanics.Angle().Value(), dq.q, dq.d, mechanics.ExternalTorque().Value() };
    }

    void PlantEvents::Configure(const SilPlantConfig& plant, uint32_t baseHz)
    {
        torqueStepTorque = foc::NewtonMeter{ plant.torqueStepNm };
        torqueStep.Configure(TicksAfter(plant.torqueStepNm != 0.0f, plant.torqueStepDelayMs, baseHz));

        const auto encoderFreezeDelay = *TicksAfter(true, uint64_t{ plant.encoderFreezeDelayCentiseconds } * 10u, baseHz);
        encoderFreeze.Configure(encoderFreezeDelay != 0 ? std::make_optional(encoderFreezeDelay) : std::nullopt);

        cableDisconnect.Configure(TicksAfter(plant.cableDisconnectDelayMs != 0, plant.cableDisconnectDelayMs, baseHz));

        supplyStepScale = plant.supplyStepScale;
        supplyStep.Configure(TicksAfter(plant.supplyStepScale > 0.0f, plant.supplyStepDelayMs, baseHz));

        const bool gripped = plant.shaftTurnRadians != 0.0f;
        const auto turnMs = std::max(plant.shaftTurnMs, uint32_t{ 1 });
        shaftTurnSpeed = foc::RadiansPerSecond{ plant.shaftTurnRadians * 1000.0f / static_cast<float>(turnMs) };
        shaftGrip.Configure(TicksAfter(gripped, plant.shaftGripDelayMs, baseHz));
        shaftHold.Configure(TicksAfter(gripped, uint64_t{ plant.shaftGripDelayMs } + turnMs, baseHz));
    }

    void PlantEvents::Begin()
    {
        torqueStep.Begin();
        encoderFreeze.Begin();
        cableDisconnect.Begin();
        supplyStep.Begin();
        shaftGrip.Begin();
        shaftHold.Begin();
    }

    void PlantEvents::Fire(uint32_t tick, foc::ThreePhaseMotorModel& model, foc::ThreePhaseMotorModel::FaultInjectionConfig& activeFaults, PlantRecorder& recorder)
    {
        if (torqueStep.Fire(tick))
        {
            model.Mechanics().SetExternalTorque(torqueStepTorque);
            recorder.Note(foc::PlantResponseKind::torqueStep, PlantSampleAt(model, tick));
        }

        if (encoderFreeze.Fire(tick))
        {
            activeFaults.encoderStuck = true;
            foc::InjectFaults(model, activeFaults);
            recorder.Note(foc::PlantResponseKind::encoderFreeze, PlantSampleAt(model, tick));
        }

        if (cableDisconnect.Fire(tick))
        {
            activeFaults.openPhaseA = activeFaults.openPhaseB = activeFaults.openPhaseC = true;
            foc::InjectFaults(model, activeFaults);
        }

        if (supplyStep.Fire(tick))
        {
            activeFaults.supplyVoltageScale = supplyStepScale;
            foc::InjectFaults(model, activeFaults);
        }

        if (shaftGrip.Fire(tick))
            model.Mechanics().Grip(shaftTurnSpeed);

        if (shaftHold.Fire(tick))
            model.Mechanics().Grip(foc::RadiansPerSecond{ 0.0f });
    }
}
