#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <cstdint>
#include <optional>

namespace virtual_motor
{
    class DisturbanceInputs
    {
    public:
        virtual void SetLoadTorque(foc::NewtonMeter torque) = 0;
        virtual void SetExternalTorque(foc::NewtonMeter torque) = 0;
        virtual void SetCurrentNoise(foc::Ampere sigma) = 0;
        virtual void SetEncoderNoise(foc::Radians sigma) = 0;

    protected:
        ~DisturbanceInputs() = default;
    };

    struct DisturbanceBaseline
    {
        foc::NewtonMeter loadTorque;
        foc::Ampere currentNoise;
        foc::Radians encoderNoise;
    };

    class PlantDisturbances
    {
    public:
        PlantDisturbances(DisturbanceInputs& inputs, const DisturbanceBaseline& baseline, uint32_t controlRateHz);

        std::optional<port::RefusalReason> SetLoadTorque(float newtonMeter);
        std::optional<port::RefusalReason> SetExternalTorque(const port::ExternalTorque& torque);
        std::optional<port::RefusalReason> SetCurrentNoise(float ampere);
        std::optional<port::RefusalReason> SetEncoderNoise(float radians);
        void ClearAll();

        std::optional<port::RefusalReason> Check(const port::ProfileStep& step) const;
        void Apply(const port::ProfileStep& step);

        void Apply();
        port::Changes InForce() const;

    private:
        std::optional<port::RefusalReason> Validate(const port::ExternalTorque& torque) const;
        float ExternalTorqueAt(float seconds) const;

        DisturbanceInputs& inputs;
        DisturbanceBaseline baseline;
        uint32_t controlRateHz;
        port::Changes inForce;
        uint64_t externalPeriods{ 0 };
        bool releaseExternalTorque{ false };
    };
}
