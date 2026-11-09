#pragma once

#include "core/foc/model/OneShotDelay.hpp"
#include "core/foc/model/PlantResponseRecorder.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/model/drive/SilPlantConfig.hpp"
#include <cstdint>

namespace sil
{
    using PlantRecorder = foc::PlantResponseRecorder<1024>;

    foc::PlantResponseSample PlantSampleAt(foc::ThreePhaseMotorModel& model, uint32_t tick);

    class PlantEvents
    {
    public:
        void Configure(const SilPlantConfig& plant, uint32_t baseHz);
        void Begin();
        void Fire(uint32_t tick, foc::ThreePhaseMotorModel& model, foc::ThreePhaseMotorModel::FaultInjectionConfig& activeFaults, PlantRecorder& recorder);

    private:
        foc::OneShotDelay torqueStep;
        foc::OneShotDelay encoderFreeze;
        foc::OneShotDelay cableDisconnect;
        foc::OneShotDelay supplyStep;
        foc::OneShotDelay shaftGrip;
        foc::OneShotDelay shaftHold;
        foc::NewtonMeter torqueStepTorque{ 0.0f };
        foc::RadiansPerSecond shaftTurnSpeed{ 0.0f };
        float supplyStepScale{ 0.0f };
    };
}
