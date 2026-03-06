#pragma once

#include "source/services/can_protocol/CanCategoryHandler.hpp"
#include "source/services/can_protocol/CanFrameCodec.hpp"
#include "source/services/can_protocol/CanProtocolDefinitions.hpp"
#include "source/services/can_protocol/CanProtocolHandler.hpp"

namespace services
{
    class CanMotorControlHandler
        : public CanCategoryHandler
    {
    public:
        explicit CanMotorControlHandler(CanProtocolHandler& protocol);

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

        CanProtocolHandler& protocol;
    };

    class CanPidTuningHandler
        : public CanCategoryHandler
    {
    public:
        explicit CanPidTuningHandler(CanProtocolHandler& protocol);

        CanCategory Category() const override;
        void Handle(CanMessageType messageType, const hal::Can::Message& data) override;

    private:
        CanProtocolHandler& protocol;
    };

    class CanMotorParametersHandler
        : public CanCategoryHandler
    {
    public:
        explicit CanMotorParametersHandler(CanProtocolHandler& protocol);

        CanCategory Category() const override;
        void Handle(CanMessageType messageType, const hal::Can::Message& data) override;

    private:
        void HandleSetPolePairs(const hal::Can::Message& data);
        void HandleSetResistance(const hal::Can::Message& data);
        void HandleSetInductance(const hal::Can::Message& data);
        void HandleSetFluxLinkage(const hal::Can::Message& data);

        CanProtocolHandler& protocol;
    };

    class CanSystemParametersHandler
        : public CanCategoryHandler
    {
    public:
        explicit CanSystemParametersHandler(CanProtocolHandler& protocol);

        CanCategory Category() const override;
        void Handle(CanMessageType messageType, const hal::Can::Message& data) override;

    private:
        CanProtocolHandler& protocol;
    };

    class CanSystemHandler
        : public CanCategoryHandler
    {
    public:
        explicit CanSystemHandler(CanProtocolHandler& protocol);

        CanCategory Category() const override;
        bool RequiresSequenceValidation() const override;
        void Handle(CanMessageType messageType, const hal::Can::Message& data) override;

    private:
        CanProtocolHandler& protocol;
    };
}
