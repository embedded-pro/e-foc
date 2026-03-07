#pragma once

#include "source/services/can_protocol/core/CanCategoryHandler.hpp"
#include "source/services/can_protocol/core/CanFrameCodec.hpp"
#include "source/services/can_protocol/core/CanProtocolDefinitions.hpp"
#include "source/services/can_protocol/server/CanProtocolServer.hpp"

namespace services
{
    class CanMotorControlHandler
        : public CanCategoryHandler
    {
    public:
        explicit CanMotorControlHandler(CanProtocolServer& protocol);

        CanCategory Category() const override;
        bool RequiresSequenceValidation() const override;
        void Handle(CanMessageType messageType, const hal::Can::Message& data) override;

    private:
        void HandleStartMotor();
        void HandleStopMotor();
        void HandleEmergencyStop();
        void HandleSetControlMode(const hal::Can::Message& data);
        void HandleSetTorqueSetpoint(const hal::Can::Message& data);
        void HandleSetSpeedSetpoint(const hal::Can::Message& data);
        void HandleSetPositionSetpoint(const hal::Can::Message& data);

        CanProtocolServer& protocol;
    };

    class CanPidTuningHandler
        : public CanCategoryHandler
    {
    public:
        explicit CanPidTuningHandler(CanProtocolServer& protocol);

        CanCategory Category() const override;
        void Handle(CanMessageType messageType, const hal::Can::Message& data) override;

    private:
        CanProtocolServer& protocol;
    };

    class CanSystemParametersHandler
        : public CanCategoryHandler
    {
    public:
        explicit CanSystemParametersHandler(CanProtocolServer& protocol);

        CanCategory Category() const override;
        void Handle(CanMessageType messageType, const hal::Can::Message& data) override;

    private:
        CanProtocolServer& protocol;
    };

    class CanSystemHandler
        : public CanCategoryHandler
    {
    public:
        explicit CanSystemHandler(CanProtocolServer& protocol);

        CanCategory Category() const override;
        bool RequiresSequenceValidation() const override;
        void Handle(CanMessageType messageType, const hal::Can::Message& data) override;

    private:
        CanProtocolServer& protocol;
    };
}
