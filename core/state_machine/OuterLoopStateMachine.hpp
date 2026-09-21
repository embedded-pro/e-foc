#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/state_machine/FocStateMachineCommon.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include <optional>

namespace application
{
    struct OuterLoopArgs
    {
        foc::Ampere maxCurrent;
        hal::Hertz baseFrequency;
        foc::LowPriorityInterrupt& lowPriorityInterrupt;
        hal::Hertz outerLoopFrequency{ 1000 };
    };

    class OuterLoopStateMachine
        : public FocStateMachineCommon
    {
    public:
        ~OuterLoopStateMachine() override = default;
        void ApplyOnlineEstimates() override;
        void TraceOnlineEstimates();

    protected:
        OuterLoopStateMachine(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices,
            foc::Ampere driveCurrentLimit);

        bool HasModeSpecificWorkPending() const override;
        void ApplyModeSpecificCalibration(const services::CalibrationData& data) override;
        bool HasValidModeSpecificCalibration(const services::CalibrationData& data) const override;
        void PrepareForEnabled() override;
        void RegisterModeSpecificCli(services::TerminalWithStorage& terminal) override;
        void RunPostAlignmentStep(state_machine::Calibrating& calibrating) override;
        void AbortModeSpecificServices() override;

        void RunMechanicalIdentStep(state_machine::Calibrating& calibrating);

        static services::MechanicalParametersIdentification& ResolveMechIdent(
            const CalibrationServices& calibServices,
            std::optional<infra::WithSharedAccess<services::MechanicalParametersIdentificationImpl>>& ownMechIdent,
            foc::SpeedCommandable& speedCommandable,
            foc::Controllable& drive,
            foc::PhaseCurrentsObservable& observable,
            drivers::ThreePhaseInverter& inverter,
            drivers::Encoder& encoder);

        virtual foc::SpeedLoopTunable& SpeedTunable() = 0;
        virtual services::MechanicalParametersIdentification& MechIdentImpl() = 0;
        virtual const services::MechanicalParametersIdentification& MechIdentImpl() const = 0;
        virtual services::RealTimeFrictionAndInertiaEstimator& GetOnlineMechEstimator() = 0;
        virtual services::RealTimeResistanceAndInductanceEstimator& GetOnlineElecEstimator() = 0;

    private:
        bool ApplyMechanics(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction, float bandwidth);
        bool ApplyIdentificationControl(const services::CalibrationData& pending);
        services::MechanicalParametersIdentification::Config ExcitationConfig() const;

        static constexpr float velocityBandwidthRadPerSec = 50.0f;

        // Bounded placeholders that make the loops able to move the rotor during identification. They are
        // deliberately small so the speed loop leans on its current envelope rather than on a plant nobody
        // has measured yet, and they are never persisted as calibration.
        static constexpr float provisionalInertia = 1.0e-4f;
        static constexpr float provisionalFriction = 0.0f;
        static constexpr float identificationBandwidthRadPerSec = 20.0f;
        static constexpr float identificationSpeedMarginFactor = 2.0f;
        // The speed loop already clamps its reference to the drive envelope; the run aborts a little above
        // that, so measurement noise at the clamp does not read as a runaway. The service clamps the result
        // down to what the inverter supports.
        static constexpr float identificationCurrentMarginFactor = 1.2f;

        foc::NewtonMeter mechTorqueConstant;
        foc::Ampere driveCurrentLimit;
    };
}
