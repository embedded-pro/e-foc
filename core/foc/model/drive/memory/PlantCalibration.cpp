#include "core/foc/model/drive/memory/PlantCalibration.hpp"
#include "numerical/math/AngleUtilities.hpp"

namespace sil
{
    namespace
    {
        constexpr float milliPerUnit{ 1000.0f };
    }

    NvmImageSpec CalibratedFor(const SilPlantConfig& plant)
    {
        NvmImageSpec spec;
        spec.calibration = CompleteCalibration();
        spec.config.currentLoop.bandwidth = calibratedCurrentLoopBandwidth;

        auto& calibration = spec.calibration;
        calibration.polePairs = plant.polePairs;
        calibration.rPhase = plant.statorResistanceOhm;
        calibration.lD = plant.dAxisInductanceHenry * milliPerUnit;
        calibration.fluxLinkage = plant.fluxLinkageWeber;
        calibration.inertia = plant.rotorInertiaKgM2;
        calibration.frictionViscous = plant.viscousDampingNmSPerRad;
        calibration.frictionCoulomb = plant.coulombFrictionNm;
        calibration.currentOffsetA = plant.noiseBiasAmpereA;
        calibration.currentOffsetB = plant.noiseBiasAmpereB;
        calibration.currentOffsetC = plant.noiseBiasAmpereC;
        if (plant.encoderIndexWidthCounts != 0)
            calibration.encoderIndexAngle = math::WrapToPi(plant.encoderIndexRadians);
        return spec;
    }
}
