#include "tools/can_commander/gui/MotorPanel.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "tools/can_commander/logic/CommandDescriptions.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QHBoxLayout>
#include <QSplitter>
#include <QVBoxLayout>

namespace tool
{
    namespace
    {
        using ui::backend::qt::ToQt;
        using ui::theme::StatusLevel;
    }

    MotorPanel::MotorPanel(CanCommandClient& client, uint16_t nodeId, Reporter reporter, QWidget* parent)
        : QWidget(parent)
        , client(client)
        , nodeId(nodeId)
        , reporter(std::move(reporter))
    {
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(CreateHeader());

        commandPanel = new CommandPanel();
        telemetryPanel = new TelemetryPanel();
        auto* splitter = new QSplitter(Qt::Horizontal);
        splitter->addWidget(commandPanel);
        splitter->addWidget(telemetryPanel);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);
        layout->addWidget(splitter, 1);

        ConnectCommands();
        ConnectSetpoints();
        Refresh();
    }

    QWidget* MotorPanel::CreateHeader()
    {
        auto* header = new QWidget();
        auto* layout = new QHBoxLayout(header);
        layout->setContentsMargins(0, 0, 0, 0);
        onlineLabel = new QLabel();
        firmwareLabel = new QLabel();
        activityLabel = new QLabel();
        layout->addWidget(onlineLabel);
        layout->addWidget(firmwareLabel);
        layout->addWidget(activityLabel);
        layout->addStretch();
        return header;
    }

    uint16_t MotorPanel::NodeId() const
    {
        return nodeId;
    }

    QString MotorPanel::Title() const
    {
        return QString("Motor %1").arg(nodeId);
    }

    QString MotorPanel::Summary() const
    {
        if (!busConnected)
            return "Not connected";
        if (!nodeOnline.has_value())
            return "No heartbeat yet";
        if (!*nodeOnline)
            return "Offline";
        if (client.Compatibility(nodeId) == can::ContractCompatibility::incompatible)
            return "Incompatible firmware";
        if (lastFault.has_value() && *lastFault != FocFaultCode::none)
            return "Fault: " + TelemetryPanel::FaultCodeName(*lastFault);
        if (lastState.has_value())
            return TelemetryPanel::MotorStateName(*lastState);
        return "Online";
    }

    StatusLevel MotorPanel::Level() const
    {
        if (!busConnected || !nodeOnline.has_value())
            return StatusLevel::Neutral;
        if (!*nodeOnline)
            return StatusLevel::Warning;
        if (client.Compatibility(nodeId) == can::ContractCompatibility::incompatible || (lastFault.has_value() && *lastFault != FocFaultCode::none) || lastState == FocMotorState::fault)
            return StatusLevel::Fault;
        if (lastState == FocMotorState::running)
            return StatusLevel::Ok;
        return StatusLevel::Neutral;
    }

    bool MotorPanel::AcceptsEmergencyStop() const
    {
        return busConnected && client.Compatibility(nodeId) != can::ContractCompatibility::incompatible;
    }

    bool MotorPanel::SendEmergencyStop()
    {
        pendingCommand = "Emergency Stop";
        return client.SendEmergencyStop(nodeId);
    }

    void MotorPanel::ConnectCommands()
    {
        connect(commandPanel, &CommandPanel::StartMotorRequested, this, [this]()
            {
                Send("Start Motor", [this]()
                    {
                        return client.SendStartMotor(nodeId);
                    });
            });
        connect(commandPanel, &CommandPanel::StopMotorRequested, this, [this]()
            {
                Send("Stop Motor", [this]()
                    {
                        return client.SendStopMotor(nodeId);
                    });
            });
        connect(commandPanel, &CommandPanel::RequestDataRequested, this, [this]()
            {
                client.RequestData(nodeId);
            });
        connect(commandPanel, &CommandPanel::SetControlModeRequested, this, [this](can::FocMotorMode mode)
            {
                commandPanel->ShowRequestedControlMode(mode);
                Send("Control Mode " + CommandPanel::ControlModeName(mode), [this, mode]()
                    {
                        return client.SendSetControlMode(nodeId, mode);
                    });
            });
    }

    void MotorPanel::ConnectSetpoints()
    {
        const auto bind = [this](void (CommandPanel::*signal)(float), const char* label, const char* unit, bool (CanCommandClient::*send)(uint16_t, float))
        {
            connect(commandPanel, signal, this, [this, label, unit, send](float value)
                {
                    Send(QString("%1 %2 %3").arg(label).arg(value).arg(unit), [this, send, value]()
                        {
                            return (client.*send)(nodeId, value);
                        });
                });
        };

        bind(&CommandPanel::SetTorqueSetpointRequested, "Torque Setpoint", "A", &CanCommandClient::SendSetTorqueSetpoint);
        bind(&CommandPanel::SetSpeedSetpointRequested, "Speed Setpoint", "rad/s", &CanCommandClient::SendSetSpeedSetpoint);
        bind(&CommandPanel::SetPositionSetpointRequested, "Position Setpoint", "rad", &CanCommandClient::SendSetPositionSetpoint);
        bind(&CommandPanel::SetCurrentBandwidthRequested, "Current Loop Bandwidth", "rad/s", &CanCommandClient::SendSetCurrentBandwidth);
        bind(&CommandPanel::SetSpeedBandwidthRequested, "Speed Loop Bandwidth", "rad/s", &CanCommandClient::SendSetSpeedBandwidth);
        bind(&CommandPanel::SetPositionBandwidthRequested, "Position Loop Bandwidth", "rad/s", &CanCommandClient::SendSetPositionBandwidth);
    }

    void MotorPanel::Send(const QString& command, const std::function<bool()>& send)
    {
        pendingCommand = command;
        if (send())
            return;

        const auto reason = client.Compatibility(nodeId) == can::ContractCompatibility::incompatible ? "its firmware contract is incompatible" : "the CAN adapter refused the frame";
        Report(IssueSeverity::error, QString("%1 was not sent: %2").arg(command, reason));
    }

    void MotorPanel::Report(IssueSeverity severity, const QString& message) const
    {
        reporter(severity, QString("%1: %2").arg(Title(), message));
    }

    void MotorPanel::ForgetNodeState()
    {
        lastState.reset();
        lastFault.reset();
        telemetryPanel->Clear();
        commandPanel->ClearActiveControlMode();
    }

    QString MotorPanel::CommandsUnavailableReason() const
    {
        if (!busConnected)
            return "Connect to a CAN interface to send commands.";
        if (nodeOnline != true)
            return QString("Node %1 is not on the bus: no heartbeat %2. Check its power, wiring and node ID.").arg(nodeId).arg(nodeOnline.has_value() ? "for 3 s" : "seen yet");

        switch (client.Compatibility(nodeId))
        {
            case can::ContractCompatibility::incompatible:
                return QString("Firmware contract %1 is incompatible with this tool (%2.x): commands are disabled. Update the firmware or the tool.")
                    .arg(contractVersion.isEmpty() ? QString("?") : contractVersion)
                    .arg(can::focContractVersionMajor);
            case can::ContractCompatibility::unknown:
                return QString("Waiting for node %1 to report its firmware version…").arg(nodeId);
            default:
                break;
        }

        if (client.IsBusy(nodeId))
            return QString("Waiting for the response to %1…").arg(pendingCommand);
        return {};
    }

    void MotorPanel::Refresh()
    {
        commandPanel->SetCommandsAvailable(CommandsUnavailableReason(), !client.IsBusy(nodeId));
        RefreshHeader();
        emit StatusChanged();
    }

    void MotorPanel::RefreshHeader()
    {
        onlineLabel->setText(QString("Node %1: %2").arg(nodeId).arg(Summary()));
        ui::backend::qt::StyleStatusLabel(*onlineLabel, Level());

        switch (client.Compatibility(nodeId))
        {
            case can::ContractCompatibility::compatible:
                firmwareLabel->setText(contractVersion.isEmpty() ? QString("Firmware compatible") : QString("Firmware contract %1").arg(contractVersion));
                ui::backend::qt::StyleStatusLabel(*firmwareLabel, StatusLevel::Ok);
                break;
            case can::ContractCompatibility::incompatible:
                firmwareLabel->setText(QString("Firmware contract %1: incompatible").arg(contractVersion));
                ui::backend::qt::StyleStatusLabel(*firmwareLabel, StatusLevel::Fault);
                break;
            default:
                firmwareLabel->setText("Firmware: unknown");
                ui::backend::qt::StyleStatusLabel(*firmwareLabel, StatusLevel::Neutral);
                break;
        }

        activityLabel->setText(client.IsBusy(nodeId) ? QString("Waiting for response to %1…").arg(pendingCommand) : QString{});
        ui::backend::qt::StyleStatusLabel(*activityLabel, StatusLevel::Warning);
    }

    void MotorPanel::SetBusConnected(bool connected)
    {
        busConnected = connected;
        nodeOnline.reset();
        ForgetNodeState();
        Refresh();
    }

    void MotorPanel::OnNodeOnlineChanged(bool online)
    {
        if (online)
            Report(IssueSeverity::info, "online");
        else
        {
            Report(IssueSeverity::warning, "stopped sending heartbeats");
            ForgetNodeState();
        }

        nodeOnline = online;
        Refresh();
    }

    void MotorPanel::OnBusyChanged(bool)
    {
        Refresh();
    }

    void MotorPanel::OnCommandTimeout()
    {
        Report(IssueSeverity::warning, QString("no response to %1 within %2 s: check that the controller is powered")
                                           .arg(pendingCommand.isEmpty() ? QString("the last command") : pendingCommand)
                                           .arg(std::chrono::duration_cast<std::chrono::seconds>(CanCommandClient::commandTimeout).count()));
    }

    void MotorPanel::OnCommandAck(uint8_t categoryId, uint8_t commandType, services::CanAckStatus status)
    {
        auto name = ToQt(CommandDisplayName(categoryId, commandType));
        if (name.isEmpty())
            name = QString("Command 0x%1/0x%2").arg(categoryId, 2, 16, QChar('0')).arg(commandType, 2, 16, QChar('0'));

        const auto severity = status == services::CanAckStatus::success ? IssueSeverity::info : IssueSeverity::warning;
        Report(severity, QString("%1 %2").arg(name, ToQt(AckStatusDescription(status))));
    }

    void MotorPanel::OnCommandRejected(uint8_t commandType, can::FocMotorCategoryError error)
    {
        auto name = ToQt(CommandDisplayName(can::focMotorCategoryId, commandType));
        if (name.isEmpty())
            name = QString("Command 0x%1").arg(commandType, 2, 16, QChar('0'));

        Report(IssueSeverity::warning, QString("%1 %2").arg(name, ToQt(RejectionDescription(error))));
        if (commandType == can::focSelectControlModeId)
            commandPanel->ClearActiveControlMode();
    }

    void MotorPanel::OnContractVersion(uint8_t major, uint8_t minor, bool compatible)
    {
        contractVersion = QString("%1.%2").arg(major).arg(minor);

        if (compatible)
            Report(IssueSeverity::info, QString("firmware contract %1 is compatible").arg(contractVersion));
        else
            Report(IssueSeverity::error, CommandsUnavailableReason());

        Refresh();
    }

    void MotorPanel::OnControlModeAcknowledged(can::FocMotorMode activeMode)
    {
        commandPanel->SetActiveControlMode(activeMode);
        Report(IssueSeverity::info, QString("control mode is now %1").arg(CommandPanel::ControlModeName(activeMode)));
    }

    void MotorPanel::OnMotorStatus(FocMotorState state, FocFaultCode fault)
    {
        telemetryPanel->OnMotorStatus(state, fault);

        const bool changed = lastState != state || lastFault != fault;
        if (lastState != state)
            Report(IssueSeverity::info, QString("state %1").arg(TelemetryPanel::MotorStateName(state)));

        if (lastFault != fault)
        {
            if (fault != FocFaultCode::none)
                Report(IssueSeverity::error, QString("fault: %1").arg(TelemetryPanel::FaultCodeName(fault)));
            else if (lastFault.has_value())
                Report(IssueSeverity::info, "fault cleared");
        }

        lastState = state;
        lastFault = fault;
        if (changed)
            Refresh();
    }

    void MotorPanel::OnCurrentMeasurement(float idCurrent, float iqCurrent)
    {
        telemetryPanel->OnCurrentMeasurement(idCurrent, iqCurrent);
    }

    void MotorPanel::OnSpeedPosition(float speed, float position)
    {
        telemetryPanel->OnSpeedPosition(speed, position);
    }

    void MotorPanel::OnBusVoltage(float voltage)
    {
        telemetryPanel->OnBusVoltage(voltage);
    }

    void MotorPanel::OnElectricalParameters(float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs)
    {
        telemetryPanel->OnElectricalParameters(resistanceOhm, inductanceMilliHenry, polePairs);
        Report(IssueSeverity::info, QString("electrical parameters R = %1 Ω, L = %2 mH, %3 pole pairs").arg(resistanceOhm).arg(inductanceMilliHenry).arg(polePairs));
    }

    void MotorPanel::OnMechanicalParameters(float frictionNmSPerRad, float inertiaKgM2)
    {
        telemetryPanel->OnMechanicalParameters(frictionNmSPerRad, inertiaKgM2);
        Report(IssueSeverity::info, QString("mechanical parameters B = %1 N·m·s/rad, J = %2 kg·m²").arg(frictionNmSPerRad).arg(inertiaKgM2));
    }
}
