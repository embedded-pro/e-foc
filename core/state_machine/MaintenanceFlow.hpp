#pragma once

#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/LifecycleContext.hpp"

namespace application
{
    class MaintenanceFlow
    {
    public:
        MaintenanceFlow(const LifecycleEnvironment& environment, const CalibrationFlow& calibration);

        void BeginClear(const state_machine::ClearCalibration& command);
        state_machine::Idle CompleteClear();
        void ReconcileClear();
        void RejectClear();

        bool IsAcceptableMotorParameter(const state_machine::SetMotorParameter& command) const;
        void BeginSetMotorParameter(const state_machine::SetMotorParameter& command);
        void AdoptMotorParameter(services::NvmStatus status);
        void CompleteMotorParameter(services::NvmStatus status);

    private:
        void OnInvalidated(services::NvmStatus status);
        void OnMotorParameterStored(services::NvmStatus status);
        services::CalibrationData Edited(const services::CalibrationData& data) const;
        bool CompletesRecord(const services::CalibrationData& data) const;

    private:
        const LifecycleEnvironment& env;
        const CalibrationFlow& calibration;
        services::MotorParameter pendingParameter{ services::MotorParameter::resistance };
        float pendingValue{ 0.0f };
    };
}
